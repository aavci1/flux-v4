#pragma once

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <functional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace llm_studio {

struct ChatMessage {
  /// `User` / `Assistant` are sent to the Ollama API. `Reasoning` is UI-only (thinking, tools, images).
  enum class Role { User, Reasoning, Assistant };
  Role role = Role::User;
  /// User message, or assistant main reply (`content` from the model).
  std::string text;
  
  constexpr bool operator==(ChatMessage const& o) const = default;
};

struct OllamaUiEvent {
  enum class Kind { Chunk, Done, Error };
  /// Which assistant stream field this chunk updates (only when \p kind == Chunk).
  enum class Part { Content, Thinking, ToolCalls, Images };
  Kind kind = Kind::Done;
  Part part = Part::Content;
  std::string text;
};

/// Emits one or more chunk events from an Ollama `message` object in a stream line.
/// Order: thinking / tools / images before main content so the UI can treat "answer" as starting after reasoning.
inline void emitChunksFromOllamaMessage(nlohmann::json const& msg, std::function<void(OllamaUiEvent)> const& post) {
  if (msg.contains("thinking") && msg["thinking"].is_string()) {
    std::string t = msg["thinking"].get<std::string>();
    if (!t.empty()) {
      post(OllamaUiEvent{.kind = OllamaUiEvent::Kind::Chunk, .part = OllamaUiEvent::Part::Thinking, .text = std::move(t)});
    }
  }

  if (msg.contains("content") && msg["content"].is_string()) {
    std::string c = msg["content"].get<std::string>();
    if (!c.empty()) {
      post(OllamaUiEvent{.kind = OllamaUiEvent::Kind::Chunk, .part = OllamaUiEvent::Part::Content, .text = std::move(c)});
    }
  }
}

inline std::string defaultOllamaBaseUrl() {
  if (char const* h = std::getenv("OLLAMA_HOST")) {
    std::string s(h);
    while (!s.empty() && (s.back() == '/' || s.back() == ' ')) {
      s.pop_back();
    }
    return s.empty() ? "http://127.0.0.1:11434" : s;
  }
  return "http://127.0.0.1:11434";
}

inline std::string defaultOllamaModel() {
  if (char const* m = std::getenv("OLLAMA_MODEL")) {
    return std::string(m);
  }
  return "gemma4";
}

inline nlohmann::json messagesToJson(std::vector<ChatMessage> const& messages) {
  auto arr = nlohmann::json::array();
  for (auto const& m : messages) {
    if (m.role == ChatMessage::Role::Reasoning) {
      continue;
    }
    arr.push_back(nlohmann::json{
        {"role", m.role == ChatMessage::Role::User ? "user" : "assistant"},
        {"content", m.text},
    });
  }
  return arr;
}

/// Drops UI-only `Reasoning` rows and a trailing empty `Assistant` placeholder for the active request.
inline std::vector<ChatMessage> messagesForApi(std::vector<ChatMessage> const& thread) {
  std::vector<ChatMessage> out;
  out.reserve(thread.size());
  for (auto const& m : thread) {
    if (m.role == ChatMessage::Role::Reasoning) {
      continue;
    }
    out.push_back(m);
  }
  while (!out.empty() && out.back().role == ChatMessage::Role::Assistant && out.back().text.empty()) {
    out.pop_back();
  }
  return out;
}

struct WriteBuf {
  std::string pending;
  std::function<void(OllamaUiEvent)> post;
};

inline size_t curlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* wb = static_cast<WriteBuf*>(userdata);
  size_t const total = size * nmemb;
  wb->pending.append(ptr, total);

  while (true) {
    auto const nl = wb->pending.find('\n');
    if (nl == std::string::npos) {
      break;
    }
    std::string line = wb->pending.substr(0, nl);
    wb->pending.erase(0, nl + 1);
    if (line.empty()) {
      continue;
    }
    try {
      nlohmann::json const j = nlohmann::json::parse(line);
      if (j.contains("message") && j["message"].is_object()) {
        emitChunksFromOllamaMessage(j["message"], wb->post);
      }
    } catch (...) {
      // Ignore malformed partial lines; Ollama streams JSON lines.
    }
  }
  return total;
}

/// Runs a streaming POST /api/chat on a detached thread; invokes \p post on the caller thread via EventQueue from main.
inline void startOllamaChatStream(std::string baseUrl, std::string model, nlohmann::json messages, std::function<void(OllamaUiEvent)> post) {
  while (!baseUrl.empty() && (baseUrl.back() == '/' || baseUrl.back() == ' ')) {
    baseUrl.pop_back();
  }
  std::string const url = baseUrl + "/api/chat";

  nlohmann::json body;
  body["model"] = std::move(model);
  body["messages"] = std::move(messages);
  // body["think"] = false;
  body["stream"] = true;
  std::string const payload = body.dump();

  std::thread([url, payload, post = std::move(post)]() mutable {
    CURL* curl = curl_easy_init();
    if (!curl) {
      post(OllamaUiEvent{.kind = OllamaUiEvent::Kind::Error, .text = "curl_easy_init failed"});
      return;
    }

    WriteBuf wb;
    wb.post = std::move(post);

    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &wb);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L);

    CURLcode const rc = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
      std::ostringstream os;
      os << "Ollama request failed: " << curl_easy_strerror(rc);
      wb.post(OllamaUiEvent{.kind = OllamaUiEvent::Kind::Error, .text = os.str()});
      return;
    }
    if (httpCode != 200) {
      std::ostringstream os;
      os << "Ollama HTTP " << httpCode << " (is the server running at " << url << "?)";
      wb.post(OllamaUiEvent{.kind = OllamaUiEvent::Kind::Error, .text = os.str()});
      return;
    }

    if (!wb.pending.empty()) {
      try {
        nlohmann::json const j = nlohmann::json::parse(wb.pending);
        if (j.contains("message") && j["message"].is_object()) {
          emitChunksFromOllamaMessage(j["message"], wb.post);
        }
      } catch (...) {
        // Last line usually ends with newline; ignore leftover garbage.
      }
    }

    wb.post(OllamaUiEvent{.kind = OllamaUiEvent::Kind::Done, .text = {}});
  }).detach();
}

} // namespace llm_studio
