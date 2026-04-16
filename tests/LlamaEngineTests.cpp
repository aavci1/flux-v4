#include <doctest/doctest.h>

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "../examples/lambda-studio/LlamaEngine.hpp"
#include "../examples/lambda-studio/Tooling.hpp"

namespace {

class ScopedEnv {
  public:
    ScopedEnv(char const *name, char const *value) : name_(name) {
        if (char const *existing = std::getenv(name); existing != nullptr) {
            hadPrevious_ = true;
            previous_ = existing;
        }
        ::setenv(name_, value, 1);
    }

    ~ScopedEnv() {
        if (hadPrevious_) {
            ::setenv(name_, previous_.c_str(), 1);
        } else {
            ::unsetenv(name_);
        }
    }

  private:
    char const *name_;
    bool hadPrevious_ = false;
    std::string previous_;
};

lambda_studio_backend::ChatGenerationRequest makeRequest(std::string chatId, std::uint64_t generationId) {
    return lambda_studio_backend::ChatGenerationRequest {
        .chatId = std::move(chatId),
        .generationId = generationId,
        .messages =
            {
                lambda_studio_backend::ChatMessage {
                    .role = lambda_studio_backend::ChatMessage::Role::User,
                    .text = "Run the fake stream.",
                },
            },
    };
}

class EventCollector {
  public:
    void post(lambda_studio_backend::LlmUiEvent event) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            events_.push_back(std::move(event));
        }
        cv_.notify_all();
    }

    bool waitFor(
        std::function<bool(std::vector<lambda_studio_backend::LlmUiEvent> const &)> const &predicate,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(8000)
    ) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [&] { return predicate(events_); });
    }

    std::vector<lambda_studio_backend::LlmUiEvent> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return events_;
    }

  private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<lambda_studio_backend::LlmUiEvent> events_;
};

bool hasChunkForChat(
    std::vector<lambda_studio_backend::LlmUiEvent> const &events,
    std::string const &chatId,
    std::uint64_t generationId
) {
    for (auto const &event : events) {
        if (event.chatId == chatId && event.generationId == generationId &&
            event.kind == lambda_studio_backend::LlmUiEvent::Kind::Chunk) {
            return true;
        }
    }
    return false;
}

std::optional<std::string> doneStatusForChat(
    std::vector<lambda_studio_backend::LlmUiEvent> const &events,
    std::string const &chatId,
    std::uint64_t generationId
) {
    for (auto it = events.rbegin(); it != events.rend(); ++it) {
        if (it->chatId != chatId || it->generationId != generationId ||
            it->kind != lambda_studio_backend::LlmUiEvent::Kind::Done || !it->generationStats.has_value()) {
            continue;
        }
        return it->generationStats->status;
    }
    return std::nullopt;
}

std::optional<lambda_studio_backend::LlmUiEvent> doneEventForChat(
    std::vector<lambda_studio_backend::LlmUiEvent> const &events,
    std::string const &chatId,
    std::uint64_t generationId
) {
    for (auto it = events.rbegin(); it != events.rend(); ++it) {
        if (it->chatId != chatId || it->generationId != generationId ||
            it->kind != lambda_studio_backend::LlmUiEvent::Kind::Done) {
            continue;
        }
        return *it;
    }
    return std::nullopt;
}

std::filesystem::path uniqueToolTempDir(char const *suffix) {
    auto const now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("lambda-studio-tool-test-" + std::to_string(now) + "-" + std::string(suffix));
}

} // namespace

TEST_CASE("LlamaEngine fake stream allows concurrent chats") {
    ScopedEnv fakeStream("LAMBDA_STUDIO_FAKE_STREAM", "1");
    ScopedEnv fakeTps("LAMBDA_STUDIO_FAKE_TOKENS_PER_SECOND", "400");

    lambda_studio_backend::LlamaEngine engine;
    EventCollector collector;

    engine.startChat(makeRequest("chat-a", 1), [&](lambda_studio_backend::LlmUiEvent event) { collector.post(std::move(event)); });
    engine.startChat(makeRequest("chat-b", 2), [&](lambda_studio_backend::LlmUiEvent event) { collector.post(std::move(event)); });

    REQUIRE(collector.waitFor([](auto const &events) {
        return doneStatusForChat(events, "chat-a", 1).has_value() &&
               doneStatusForChat(events, "chat-b", 2).has_value();
    }));

    auto events = collector.snapshot();
    CHECK(hasChunkForChat(events, "chat-a", 1));
    CHECK(hasChunkForChat(events, "chat-b", 2));
    CHECK(doneStatusForChat(events, "chat-a", 1) == std::optional<std::string>("completed"));
    CHECK(doneStatusForChat(events, "chat-b", 2) == std::optional<std::string>("completed"));
}

TEST_CASE("LlamaEngine fake stream cancelChat only stops the targeted chat") {
    ScopedEnv fakeStream("LAMBDA_STUDIO_FAKE_STREAM", "1");
    ScopedEnv fakeTps("LAMBDA_STUDIO_FAKE_TOKENS_PER_SECOND", "80");

    lambda_studio_backend::LlamaEngine engine;
    EventCollector collector;

    engine.startChat(makeRequest("chat-a", 1), [&](lambda_studio_backend::LlmUiEvent event) { collector.post(std::move(event)); });
    engine.startChat(makeRequest("chat-b", 2), [&](lambda_studio_backend::LlmUiEvent event) { collector.post(std::move(event)); });

    REQUIRE(collector.waitFor([](auto const &events) {
        return hasChunkForChat(events, "chat-a", 1) && hasChunkForChat(events, "chat-b", 2);
    }));

    engine.cancelChat("chat-a");

    REQUIRE(collector.waitFor([](auto const &events) {
        return doneStatusForChat(events, "chat-a", 1).has_value() &&
               doneStatusForChat(events, "chat-b", 2).has_value();
    }));

    auto events = collector.snapshot();
    CHECK(doneStatusForChat(events, "chat-a", 1) == std::optional<std::string>("cancelled"));
    CHECK(doneStatusForChat(events, "chat-b", 2) == std::optional<std::string>("completed"));
}

TEST_CASE("LlamaEngine fake stream cancelAllGenerations stops every chat") {
    ScopedEnv fakeStream("LAMBDA_STUDIO_FAKE_STREAM", "1");
    ScopedEnv fakeTps("LAMBDA_STUDIO_FAKE_TOKENS_PER_SECOND", "80");

    lambda_studio_backend::LlamaEngine engine;
    EventCollector collector;

    engine.startChat(makeRequest("chat-a", 1), [&](lambda_studio_backend::LlmUiEvent event) { collector.post(std::move(event)); });
    engine.startChat(makeRequest("chat-b", 2), [&](lambda_studio_backend::LlmUiEvent event) { collector.post(std::move(event)); });

    REQUIRE(collector.waitFor([](auto const &events) {
        return hasChunkForChat(events, "chat-a", 1) && hasChunkForChat(events, "chat-b", 2);
    }));

    engine.cancelAllGenerations();

    REQUIRE(collector.waitFor([](auto const &events) {
        return doneStatusForChat(events, "chat-a", 1).has_value() &&
               doneStatusForChat(events, "chat-b", 2).has_value();
    }));

    auto events = collector.snapshot();
    CHECK(doneStatusForChat(events, "chat-a", 1) == std::optional<std::string>("cancelled"));
    CHECK(doneStatusForChat(events, "chat-b", 2) == std::optional<std::string>("cancelled"));
}

TEST_CASE("LlamaEngine applies request over chat over engine generation defaults") {
    ScopedEnv fakeStream("LAMBDA_STUDIO_FAKE_STREAM", "1");
    ScopedEnv fakeTps("LAMBDA_STUDIO_FAKE_TOKENS_PER_SECOND", "300");

    lambda_studio_backend::LlamaEngine engine;
    EventCollector collector;

    auto engineResult = engine.updateGenerationDefaults(lambda_studio_backend::GenerationParamsPatch {.temp = 0.6f});
    CHECK(engineResult.scope == lambda_studio_backend::ApplyScope::AppliedImmediately);
    auto chatResult =
        engine.updateChatGenerationParams("chat-a", lambda_studio_backend::GenerationParamsPatch {.temp = 0.9f});
    CHECK(chatResult.scope == lambda_studio_backend::ApplyScope::AppliedImmediately);

    auto request = makeRequest("chat-a", 42);
    request.requestGenerationParams = lambda_studio_backend::GenerationParams {.temp = 0.2f};
    engine.startChat(std::move(request), [&](lambda_studio_backend::LlmUiEvent event) { collector.post(std::move(event)); });

    REQUIRE(collector.waitFor([](auto const &events) {
        return doneStatusForChat(events, "chat-a", 42).has_value();
    }));

    auto doneEvent = doneEventForChat(collector.snapshot(), "chat-a", 42);
    REQUIRE(doneEvent.has_value());
    REQUIRE(doneEvent->generationStats.has_value());
    CHECK(doctest::Approx(doneEvent->generationStats->temp).epsilon(0.001f) == 0.2f);
}

TEST_CASE("LlamaEngine defers load/session updates while generation is active") {
    ScopedEnv fakeStream("LAMBDA_STUDIO_FAKE_STREAM", "1");
    ScopedEnv fakeTps("LAMBDA_STUDIO_FAKE_TOKENS_PER_SECOND", "40");

    lambda_studio_backend::LlamaEngine engine;
    EventCollector collector;

    engine.startChat(makeRequest("chat-a", 1), [&](lambda_studio_backend::LlmUiEvent event) { collector.post(std::move(event)); });
    REQUIRE(collector.waitFor([](auto const &events) { return hasChunkForChat(events, "chat-a", 1); }));

    auto loadResult = engine.updateLoadParams(lambda_studio_backend::LoadParamsPatch {
        .modelPath = std::string("/tmp/placeholder.gguf"),
        .nGpuLayers = 3,
    });
    auto sessionResult = engine.updateSessionDefaults(lambda_studio_backend::SessionParamsPatch {.nCtx = 4096});

    CHECK(loadResult.scope == lambda_studio_backend::ApplyScope::Deferred);
    CHECK(sessionResult.scope == lambda_studio_backend::ApplyScope::Deferred);

    engine.cancelAllGenerations();
}

TEST_CASE("Tooling invokes workspace-bounded read_file") {
    std::filesystem::path const tempDir = uniqueToolTempDir("read-file");
    std::filesystem::create_directories(tempDir);
    std::filesystem::path const filePath = tempDir / "sample.txt";
    {
        std::ofstream stream(filePath);
        stream << "alpha\nbeta\ngamma\n";
    }

    lambda_studio_backend::ToolConfig config;
    config.workspaceRoot = tempDir.string();

    auto const result = lambda_studio_backend::tooling::invokeTool(
        config,
        "read_file",
        R"({"path":"sample.txt","start_line":2,"end_line":3})"
    );
    CHECK(result.value("ok", false));
    CHECK(result.value("tool", std::string()) == "read_file");
    CHECK(result.value("plain_text_response", std::string()) == "beta\ngamma\n");

    std::filesystem::remove_all(tempDir);
}

TEST_CASE("Tooling requires approval only for shell execution") {
    lambda_studio_backend::ToolConfig config;
    CHECK(!lambda_studio_backend::tooling::requiresApproval(config, "read_file"));
    CHECK(!lambda_studio_backend::tooling::requiresApproval(config, "grep_search"));
    CHECK(lambda_studio_backend::tooling::requiresApproval(config, "exec_shell_command"));
}

TEST_CASE("Tool workspace root normalization upgrades legacy build-directory roots") {
    std::filesystem::path const tempDir = uniqueToolTempDir("workspace-root");
    std::filesystem::path const buildExamples = tempDir / "build" / "examples";
    std::filesystem::create_directories(buildExamples);
    std::filesystem::create_directories(tempDir / ".git");

    std::string const normalized =
        lambda_studio_backend::normalizeToolWorkspaceRoot(buildExamples.string());
    CHECK(normalized == tempDir.string());

    std::filesystem::remove_all(tempDir);
}

TEST_CASE("Structured tool turns invalidate reusable session prefixes") {
    lambda_studio_backend::ChatGenerationRequest request {
        .chatId = "chat-tools",
        .generationId = 7,
    };

    CHECK(!lambda_studio_backend::detail::requestNeedsStructuredToolReset(request));

    request.messages.push_back(lambda_studio_backend::ChatMessage {
        .role = lambda_studio_backend::ChatMessage::Role::Assistant,
        .text = "Final answer",
    });
    CHECK(!lambda_studio_backend::detail::requestNeedsStructuredToolReset(request));

    request.messages.back() = lambda_studio_backend::ChatMessage {
        .role = lambda_studio_backend::ChatMessage::Role::Assistant,
        .toolCalls = {
            lambda_studio_backend::ToolCall {
                .id = "call_1",
                .name = "read_file",
                .arguments = R"({"path":"README.md"})",
            },
        },
    };
    CHECK(lambda_studio_backend::detail::requestNeedsStructuredToolReset(request));

    request.messages.push_back(lambda_studio_backend::ChatMessage {
        .role = lambda_studio_backend::ChatMessage::Role::Tool,
        .text = R"({"ok":true,"tool":"read_file","plain_text_response":"hello"})",
        .toolCallId = "call_1",
        .toolName = "read_file",
        .toolState = lambda_studio_backend::ToolExecutionState::Completed,
    });
    CHECK(lambda_studio_backend::detail::requestNeedsStructuredToolReset(request));
}
