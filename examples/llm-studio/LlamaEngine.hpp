#pragma once

/// \file LlamaEngine.hpp
///
/// In-process llama.cpp inference engine for LLM Studio.
/// Based on the llama-cli (b8680) architecture: chat-template-driven prompt
/// formatting, thinking-tag detection for reasoning models, and a robust
/// sampler chain. A worker std::thread runs the decode loop; tokens are
/// dispatched to the main thread via GCD → EventQueue::post.

#include "llama-cpp.h"
#include "llama.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#endif

#include "Types.hpp"

namespace llm_studio {

// ── UI event ────────────────────────────────────────────────────────────────

struct LlmUiEvent {
  enum class Kind { Chunk, Done, Error };
  enum class Part { Content, Thinking };
  Kind kind = Kind::Done;
  Part part = Part::Content;
  std::string chatId;
  std::string text;
};

// ── Main-thread dispatch ────────────────────────────────────────────────────

namespace detail {

struct MainPostThunk {
  std::function<void(LlmUiEvent)> post;
  LlmUiEvent ev;
};

#if defined(__APPLE__)
inline void invokePostOnMain(void* ctx) {
  auto* p = static_cast<MainPostThunk*>(ctx);
  p->post(std::move(p->ev));
  delete p;
}
#endif

inline void postEventToMain(std::function<void(LlmUiEvent)> const& post, LlmUiEvent ev) {
#if defined(__APPLE__)
  auto* ctx = new MainPostThunk{post, std::move(ev)};
  dispatch_async_f(dispatch_get_main_queue(), ctx, invokePostOnMain);
#else
  post(std::move(ev));
#endif
}

} // namespace detail

// ── Token → string helper ───────────────────────────────────────────────────

inline std::string tokenToPiece(const llama_vocab* vocab, llama_token token) {
  char buf[256];
  int n = llama_token_to_piece(vocab, token, buf, sizeof(buf), 0, /*special=*/true);
  if (n < 0) {
    std::string s(static_cast<std::size_t>(-n), '\0');
    llama_token_to_piece(vocab, token, s.data(), static_cast<int32_t>(s.size()), 0, /*special=*/true);
    return s;
  }
  return std::string(buf, static_cast<std::size_t>(n));
}

// ── Thinking-tag parser ─────────────────────────────────────────────────────
// Detects thinking blocks in the streamed token text, mirroring the
// reasoning_content_delta / content_delta split that llama-cli's
// task_result_state::update_chat_msg performs via common_chat_parse.
//
// Supported formats (from chat.cpp b8680):
//   - Gemma 4 channels:  <|channel>thought\n … <channel|>
//   - Paired tags:       <think> … </think>  (DeepSeek-R1, Qwen3, etc.)
//   - Toggle token:      <|think|>           (generic)

struct ThinkingParser {
  struct Segment {
    LlmUiEvent::Part part;
    std::string text;
  };

  bool inThinking = false;
  std::string buffer;

  std::vector<Segment> feed(std::string const& piece) {
    buffer += piece;
    std::vector<Segment> out;
    flush(out);
    return out;
  }

  std::vector<Segment> finish() {
    std::vector<Segment> out;
    if (!buffer.empty()) {
      out.push_back({currentPart(), std::move(buffer)});
      buffer.clear();
    }
    return out;
  }

private:
  LlmUiEvent::Part currentPart() const {
    return inThinking ? LlmUiEvent::Part::Thinking : LlmUiEvent::Part::Content;
  }

  struct TagHit {
    size_t pos = std::string::npos;
    size_t len = 0;
  };

  void flush(std::vector<Segment>& out) {
    for (;;) {
      TagHit best = findNextTag();
      if (best.pos != std::string::npos) {
        if (best.pos > 0) {
          out.push_back({currentPart(), buffer.substr(0, best.pos)});
        }
        buffer = buffer.substr(best.pos + best.len);
        inThinking = !inThinking;
        continue;
      }
      size_t keep = maxPartialSuffix();
      if (keep < buffer.size()) {
        out.push_back({currentPart(), buffer.substr(0, buffer.size() - keep)});
        buffer = buffer.substr(buffer.size() - keep);
      }
      break;
    }
  }

  TagHit findNextTag() const {
    TagHit best;
    auto check = [&](std::string const& tag) {
      auto pos = buffer.find(tag);
      if (pos < best.pos) { best = {pos, tag.size()}; }
    };
    check(kToggle);
    if (!inThinking) {
      check(kOpen);
      check(kChannelOpen);
    } else {
      check(kClose);
      check(kChannelClose);
    }
    return best;
  }

  size_t maxPartialSuffix() const {
    size_t keep = partialSuffix(buffer, kToggle);
    if (!inThinking) {
      keep = std::max(keep, partialSuffix(buffer, kOpen));
      keep = std::max(keep, partialSuffix(buffer, kChannelOpen));
    } else {
      keep = std::max(keep, partialSuffix(buffer, kClose));
      keep = std::max(keep, partialSuffix(buffer, kChannelClose));
    }
    return keep;
  }

  static inline const std::string kOpen         = "<think>";
  static inline const std::string kClose        = "</think>";
  static inline const std::string kToggle       = "<|think|>";
  static inline const std::string kChannelOpen  = "<|channel>thought\n";
  static inline const std::string kChannelClose = "<channel|>";

  static size_t partialSuffix(std::string const& text, std::string const& tag) {
    size_t maxLen = std::min(text.size(), tag.size() - 1);
    for (size_t len = maxLen; len > 0; --len) {
      if (text.compare(text.size() - len, len, tag, 0, len) == 0) {
        return len;
      }
    }
    return 0;
  }
};

// ── Message → prompt tokenization ───────────────────────────────────────────

/// Filters out Reasoning messages and trailing empty Assistant placeholders.
inline std::vector<ChatMessage> messagesForApi(std::vector<ChatMessage> const& thread) {
  std::vector<ChatMessage> out;
  out.reserve(thread.size());
  for (auto const& m : thread) {
    if (m.role == ChatMessage::Role::Reasoning) continue;
    out.push_back(m);
  }
  while (!out.empty() && out.back().role == ChatMessage::Role::Assistant && out.back().text.empty())
    out.pop_back();
  return out;
}

/// Tokenize a plain text string (no special token injection).
inline std::vector<llama_token> tokenizeText(const llama_vocab* vocab,
                                              std::string const& text) {
  int n = -llama_tokenize(vocab, text.c_str(), static_cast<int32_t>(text.size()),
                          nullptr, 0, /*add_special=*/false, /*parse_special=*/false);
  if (n <= 0) return {};
  std::vector<llama_token> toks(static_cast<std::size_t>(n));
  llama_tokenize(vocab, text.c_str(), static_cast<int32_t>(text.size()),
                 toks.data(), n, false, false);
  return toks;
}

/// Tokenize a template-formatted prompt string (with parse_special=true so
/// that the special-token strings emitted by llama_chat_apply_template are
/// converted back to their token IDs).
inline std::vector<llama_token> tokenizeTemplate(const llama_vocab* vocab,
                                                  std::string const& text,
                                                  bool addBos) {
  int n = -llama_tokenize(vocab, text.c_str(), static_cast<int32_t>(text.size()),
                          nullptr, 0, /*add_special=*/addBos, /*parse_special=*/true);
  if (n <= 0) return {};
  std::vector<llama_token> toks(static_cast<std::size_t>(n));
  llama_tokenize(vocab, text.c_str(), static_cast<int32_t>(text.size()),
                 toks.data(), n, addBos, true);
  return toks;
}

struct PromptBuildResult {
  std::vector<llama_token> tokens;
  bool thinkingStarted = false;
};

/// Build the prompt token sequence for the given conversation.
///
/// Strategy:
///   1. Try llama_chat_apply_template (only works for templates with
///      hardcoded C++ support — NOT Jinja2 like Gemma 4).
///   2. Fallback: construct the token sequence directly using the model's
///      actual special-token IDs. Tokens are found by scanning the first
///      512 vocab entries for known piece strings.
///
/// For thinking-mode models (Gemma 4), the generation prompt ends with
/// <|channel>thought\n  (the Gemma 4 "thought channel" prefix, from chat.cpp).
/// The returned thinkingStarted flag tells the caller to start the
/// ThinkingParser in thinking-mode.
inline PromptBuildResult buildPromptTokens(const llama_model* model,
                                            const llama_vocab* vocab,
                                            std::vector<ChatMessage> const& messages) {
  bool const addBos = llama_vocab_get_add_bos(vocab);

  // ── 1. Chat template path ────────────────────────────────────────────────
  std::vector<llama_chat_message> chat;
  chat.reserve(messages.size());
  for (auto const& m : messages) {
    chat.push_back(llama_chat_message{
        .role    = m.role == ChatMessage::Role::User ? "user" : "assistant",
        .content = m.text.c_str(),
    });
  }

  const char* modelTmpl = llama_model_chat_template(model, nullptr);
  if (modelTmpl) {
    int32_t len = llama_chat_apply_template(modelTmpl, chat.data(), chat.size(),
                                            true, nullptr, 0);
    if (len > 0) {
      std::string fmtStr(static_cast<std::size_t>(len), '\0');
      llama_chat_apply_template(modelTmpl, chat.data(), chat.size(),
                                true, fmtStr.data(), len + 1);
      auto toks = tokenizeTemplate(vocab, fmtStr, addBos);
      std::fprintf(stderr, "[LlamaEngine] chat template ok: %d chars -> %zu tokens\n",
                   len, toks.size());
      return {std::move(toks), false};
    }
    std::fprintf(stderr, "[LlamaEngine] chat template apply failed (len=%d), using fallback\n",
                 len);
  } else {
    std::fprintf(stderr, "[LlamaEngine] no model template, using fallback\n");
  }

  // ── 2. Direct token-sequence fallback ────────────────────────────────────
  // Scan the first 512 tokens for known special-token pieces.
  llama_token sotTok        = LLAMA_TOKEN_NULL;   // <|turn>  or <start_of_turn>
  llama_token eotTok        = LLAMA_TOKEN_NULL;   // <turn|>  or <end_of_turn>
  llama_token channelOpen   = LLAMA_TOKEN_NULL;   // <|channel>   (Gemma 4)
  llama_token channelClose  = LLAMA_TOKEN_NULL;   // <channel|>   (Gemma 4)
  llama_token thinkTok      = LLAMA_TOKEN_NULL;   // <|think|>    (generic toggle)
  for (llama_token t = 0; t < 512; ++t) {
    std::string piece = tokenToPiece(vocab, t);
    if      (piece == "<|turn>"  || piece == "<start_of_turn>")  sotTok       = t;
    else if (piece == "<turn|>"  || piece == "<end_of_turn>")    eotTok       = t;
    else if (piece == "<|channel>")                              channelOpen  = t;
    else if (piece == "<channel|>")                              channelClose = t;
    else if (piece == "<|think|>")                               thinkTok     = t;
  }

  std::fprintf(stderr, "[LlamaEngine] fallback tokens: sot=%d eot=%d chOpen=%d chClose=%d think=%d\n",
               (int)sotTok, (int)eotTok, (int)channelOpen, (int)channelClose, (int)thinkTok);

  std::vector<llama_token> toks;
  if (addBos) {
    llama_token bos = llama_vocab_bos(vocab);
    if (bos != LLAMA_TOKEN_NULL) toks.push_back(bos);
  }

  auto append = [&](std::vector<llama_token> v) {
    toks.insert(toks.end(), v.begin(), v.end());
  };

  static const std::string kNL = "\n";
  for (auto const& m : messages) {
    if (sotTok != LLAMA_TOKEN_NULL) toks.push_back(sotTok);
    append(tokenizeText(vocab, m.role == ChatMessage::Role::User ? "user" : "model"));
    append(tokenizeText(vocab, kNL));
    append(tokenizeText(vocab, m.text));
    if (eotTok != LLAMA_TOKEN_NULL) toks.push_back(eotTok);
    append(tokenizeText(vocab, kNL));
  }

  // Generation prefix: <|turn>model\n
  if (sotTok != LLAMA_TOKEN_NULL) toks.push_back(sotTok);
  append(tokenizeText(vocab, "model"));
  append(tokenizeText(vocab, kNL));

  // Enter thinking mode using the correct method for the model:
  //   Gemma 4:  <|channel>thought\n   (channel-based, from chat.cpp)
  //   Generic:  <|think|>             (toggle token)
  bool thinkingStarted = false;
  if (channelOpen != LLAMA_TOKEN_NULL) {
    toks.push_back(channelOpen);
    append(tokenizeText(vocab, "thought"));
    append(tokenizeText(vocab, kNL));
    thinkingStarted = true;
    std::fprintf(stderr, "[LlamaEngine] injected thinking channel prefix: <|channel>thought\\n\n");
  } else if (thinkTok != LLAMA_TOKEN_NULL) {
    toks.push_back(thinkTok);
    thinkingStarted = true;
    std::fprintf(stderr, "[LlamaEngine] injected <|think|> toggle\n");
  }

  std::fprintf(stderr, "[LlamaEngine] fallback prompt: %zu tokens, thinkingStarted=%d\n",
               toks.size(), (int)thinkingStarted);
  return {std::move(toks), thinkingStarted};
}

// ── Engine ──────────────────────────────────────────────────────────────────

/// Holds the loaded model and provides chat completion on a worker thread.
/// One instance per application lifetime. Thread-safe for concurrent chat requests
/// (only one generation runs at a time; new requests cancel the previous one).
class LlamaEngine {
public:
  LlamaEngine() = default;
  ~LlamaEngine() {
    cancelGeneration();
    joinWorker();
    unload();
  }

  LlamaEngine(LlamaEngine const&) = delete;
  LlamaEngine& operator=(LlamaEngine const&) = delete;

  /// Load a GGUF model file. Blocks until the model is ready.
  /// Returns true on success.
  bool load(std::string const& modelPath, int nGpuLayers = -1, uint32_t nCtx = 8192) {
    std::lock_guard<std::mutex> lock(mutex_);
    unloadLocked();

    ggml_backend_load_all();
    llama_backend_init();

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = nGpuLayers;

    model_.reset(llama_model_load_from_file(modelPath.c_str(), mparams));
    if (!model_) {
      std::fprintf(stderr, "[LlamaEngine] failed to load model: %s\n", modelPath.c_str());
      return false;
    }

    vocab_        = llama_model_get_vocab(model_.get());
    nCtx_         = nCtx;
    nGpuLayers_   = nGpuLayers;
    modelPath_    = modelPath;

    std::fprintf(stderr, "[LlamaEngine] loaded: %s (n_gpu_layers=%d, n_ctx=%u)\n",
                 modelPath.c_str(), nGpuLayers, nCtx);
    return true;
  }

  bool isLoaded() const { return model_ != nullptr; }

  std::string const& modelPath() const { return modelPath_; }

  void unload() {
    std::lock_guard<std::mutex> lock(mutex_);
    unloadLocked();
  }

  void cancelGeneration() {
    cancelled_.store(true, std::memory_order_relaxed);
  }

  /// Start a streaming chat completion on a worker thread.
  /// `post` is called on the main thread (via GCD) for each token and on completion.
  void startChat(std::vector<ChatMessage> messages, std::string chatId,
                 std::function<void(LlmUiEvent)> post) {
    if (!model_) {
      detail::postEventToMain(post, LlmUiEvent{
          .kind = LlmUiEvent::Kind::Error,
          .chatId = chatId,
          .text = "No model loaded",
      });
      return;
    }

    cancelGeneration();
    joinWorker();

    cancelled_.store(false, std::memory_order_relaxed);

    worker_ = std::thread([this,
                            msgs = std::move(messages),
                            chatId = std::move(chatId),
                            userPost = std::move(post)]() mutable {
      runGeneration(std::move(msgs), std::move(chatId), std::move(userPost));
    });
  }

private:
  void joinWorker() {
    if (worker_.joinable()) worker_.join();
  }

  void unloadLocked() {
    cancelGeneration();
    joinWorker();
    vocab_      = nullptr;
    nGpuLayers_ = -1;
    model_.reset();
    modelPath_.clear();
  }

  void runGeneration(std::vector<ChatMessage> messages, std::string chatId,
                     std::function<void(LlmUiEvent)> userPost) {
    auto postOnMain = [&userPost, &chatId](LlmUiEvent ev) {
      ev.chatId = chatId;
      detail::postEventToMain(userPost, std::move(ev));
    };

    // ── 1. Apply chat template and tokenize ─────────────────────────────
    std::vector<ChatMessage> apiMsgs = messagesForApi(messages);
    auto promptResult = buildPromptTokens(model_.get(), vocab_, apiMsgs);
    auto& promptTokens = promptResult.tokens;

    if (promptTokens.empty()) {
      postOnMain(LlmUiEvent{.kind = LlmUiEvent::Kind::Error, .text = "Empty prompt after tokenization"});
      return;
    }

    int const nPrompt = static_cast<int>(promptTokens.size());
    int const nPredict = 4096;

    std::fprintf(stderr, "[LlamaEngine] prompt: %d tokens, max predict: %d\n", nPrompt, nPredict);

    // Diagnostic: dump prompt tokens
    std::fprintf(stderr, "[LlamaEngine] prompt tokens: ");
    for (int j = 0; j < std::min(nPrompt, 30); ++j) {
      std::fprintf(stderr, "%d ", (int)promptTokens[static_cast<size_t>(j)]);
    }
    if (nPrompt > 30) std::fprintf(stderr, "...");
    std::fprintf(stderr, "\n");
    std::fprintf(stderr, "[LlamaEngine] prompt text: ");
    for (int j = 0; j < std::min(nPrompt, 30); ++j) {
      std::fprintf(stderr, "|%s", tokenToPiece(vocab_, promptTokens[static_cast<size_t>(j)]).c_str());
    }
    std::fprintf(stderr, "|\n");

    // ── 2. Create context ───────────────────────────────────────────────
    uint32_t ctxSize = std::max(nCtx_, static_cast<uint32_t>(nPrompt + nPredict));

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx   = ctxSize;
    cparams.n_batch = static_cast<uint32_t>(nPrompt);
    cparams.no_perf = true;

    llama_context_ptr ctx(llama_init_from_model(model_.get(), cparams));
    if (!ctx) {
      postOnMain(LlmUiEvent{.kind = LlmUiEvent::Kind::Error, .text = "Failed to create llama context"});
      return;
    }

    // ── 3. Sampler chain (matches llama-cli defaults) ───────────────────
    auto sparams = llama_sampler_chain_default_params();
    sparams.no_perf = true;

    llama_sampler_ptr smpl(llama_sampler_chain_init(sparams));
    llama_sampler_chain_add(smpl.get(), llama_sampler_init_penalties(64, 1.0f, 0.0f, 0.0f));
    llama_sampler_chain_add(smpl.get(), llama_sampler_init_top_k(40));
    llama_sampler_chain_add(smpl.get(), llama_sampler_init_top_p(0.95f, 1));
    llama_sampler_chain_add(smpl.get(), llama_sampler_init_min_p(0.05f, 1));
    llama_sampler_chain_add(smpl.get(), llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(smpl.get(), llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    // ── 4. Prompt evaluation (prefill) ──────────────────────────────────
    llama_batch batch = llama_batch_get_one(promptTokens.data(), nPrompt);

    if (llama_decode(ctx.get(), batch) != 0) {
      postOnMain(LlmUiEvent{.kind = LlmUiEvent::Kind::Error,
          .text = decodeError("prompt evaluation")});
      return;
    }

    // ── 5. Token generation loop with thinking-tag detection ────────────
    ThinkingParser parser;
    parser.inThinking = promptResult.thinkingStarted;
    std::fprintf(stderr, "[LlamaEngine] parser.inThinking=%d (from prompt)\n",
                 (int)parser.inThinking);
    std::string rawAccum;

    for (int i = 0; i < nPredict; ++i) {
      if (cancelled_.load(std::memory_order_relaxed)) {
        for (auto& seg : parser.finish()) {
          postOnMain(LlmUiEvent{.kind = LlmUiEvent::Kind::Chunk, .part = seg.part, .text = std::move(seg.text)});
        }
        postOnMain(LlmUiEvent{.kind = LlmUiEvent::Kind::Done});
        return;
      }

      llama_token newToken = llama_sampler_sample(smpl.get(), ctx.get(), -1);

      if (llama_vocab_is_eog(vocab_, newToken)) {
        std::fprintf(stderr, "[LlamaEngine] EOG token=%d at position %d\n", (int)newToken, i);
        break;
      }

      std::string piece = tokenToPiece(vocab_, newToken);

      // Diagnostic: log first 50 tokens with their IDs and piece text
      if (i < 50) {
        std::fprintf(stderr, "[tok %3d] id=%-6d piece=|%s|\n", i, (int)newToken, piece.c_str());
      }
      rawAccum += piece;

      auto segments = parser.feed(piece);

      for (auto& seg : segments) {
        postOnMain(LlmUiEvent{
            .kind = LlmUiEvent::Kind::Chunk,
            .part = seg.part,
            .text = std::move(seg.text),
        });
      }

      batch = llama_batch_get_one(&newToken, 1);

      if (llama_decode(ctx.get(), batch) != 0) {
        postOnMain(LlmUiEvent{.kind = LlmUiEvent::Kind::Error,
            .text = decodeError("generation")});
        return;
      }
    }

    // Diagnostic: log raw output summary
    std::fprintf(stderr, "[LlamaEngine] generation done. parser.inThinking=%d buffer_len=%zu\n",
                 (int)parser.inThinking, parser.buffer.size());
    std::fprintf(stderr, "[LlamaEngine] raw output (first 500 chars):\n%.500s\n[END]\n",
                 rawAccum.c_str());

    for (auto& seg : parser.finish()) {
      postOnMain(LlmUiEvent{.kind = LlmUiEvent::Kind::Chunk, .part = seg.part, .text = std::move(seg.text)});
    }
    postOnMain(LlmUiEvent{.kind = LlmUiEvent::Kind::Done});
  }

  std::string decodeError(char const* phase) const {
    std::string msg = std::string("Decode failed during ") + phase + ".";
    if (nGpuLayers_ != 0) {
      msg += " If you see a GPU Hang in the logs, this model's Metal kernels"
             " are unsupported on your device. Re-run with"
             " LLAMA_N_GPU_LAYERS=0 to use CPU-only inference.";
    }
    return msg;
  }

  std::mutex mutex_;
  llama_model_ptr model_;
  const llama_vocab* vocab_ = nullptr;
  int      nGpuLayers_ = -1;
  uint32_t nCtx_       = 8192;
  std::string modelPath_;
  std::atomic<bool> cancelled_{false};
  std::thread worker_;
};

// ── Defaults (env vars) ─────────────────────────────────────────────────────

inline std::string defaultModelPath() {
  if (char const* p = std::getenv("LLAMA_MODEL_PATH"))
    return std::string(p);
  return {};
}

inline std::string defaultModelName() {
  if (char const* n = std::getenv("LLAMA_MODEL_NAME"))
    return std::string(n);
  return "local";
}

/// Number of model layers to offload to GPU.
/// -1 (default) = all layers on GPU.
///  0            = CPU-only inference (required for models whose Metal kernels
///                 hang on the current device, e.g. Gemma 4 on Homebrew ggml 0.9.x).
/// Set via the LLAMA_N_GPU_LAYERS environment variable.
inline int defaultNGpuLayers() {
  if (char const* s = std::getenv("LLAMA_N_GPU_LAYERS"))
    return std::atoi(s);
  return -1;
}

} // namespace llm_studio
