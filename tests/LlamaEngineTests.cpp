#include <doctest/doctest.h>

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "../examples/lambda-studio/LlamaEngine.hpp"

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
