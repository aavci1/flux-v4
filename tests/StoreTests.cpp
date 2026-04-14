#include <doctest/doctest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "../examples/lambda-studio/Store.hpp"

namespace {

std::filesystem::path uniqueTempDir(char const *suffix) {
    auto const now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("lambda-studio-store-test-" + std::to_string(now) + "-" + std::string(suffix));
}

lambda::ChatMessage makeUserMessage(std::string text) {
    return lambda::ChatMessage {
        .role = lambda::ChatRole::User,
        .text = std::move(text),
    };
}

lambda::MessageGenerationStats makeGenerationStats(std::string modelName, std::int64_t completionTokens) {
    return lambda::MessageGenerationStats {
        .modelPath = "/tmp/model.gguf",
        .modelName = std::move(modelName),
        .promptTokens = 42,
        .completionTokens = completionTokens,
        .startedAtUnixMs = 1'000,
        .firstTokenAtUnixMs = 1'020,
        .finishedAtUnixMs = 1'420,
        .tokensPerSecond = 50.0,
        .status = "completed",
        .errorText = "",
        .temp = 0.7f,
        .topP = 0.95f,
        .topK = 40,
        .maxTokens = 512,
    };
}

} // namespace

TEST_CASE("Store incremental chat persistence updates only targeted thread") {
    std::filesystem::path const tempDir = uniqueTempDir("thread");
    std::filesystem::create_directories(tempDir);

    lambda::Store store(tempDir);
    store.upsertChatThreadMeta("chat-a", "Chat A", 100, "/tmp/a.gguf", "A", 0);
    store.upsertChatThreadMeta("chat-b", "Chat B", 100, "/tmp/b.gguf", "B", 1);
    store.replaceChatMessagesForThread("chat-a", {makeUserMessage("A1"), makeUserMessage("A2")});
    store.replaceChatMessagesForThread("chat-b", {makeUserMessage("B1"), makeUserMessage("B2")});
    store.updateSelectedChatId("chat-a");

    store.replaceChatMessagesForThread("chat-a", {makeUserMessage("A3")});

    lambda::PersistedChatState const state = store.loadPersistedChatState();
    REQUIRE(state.chats.size() == 2);
    REQUIRE(state.chats[0].id == "chat-a");
    REQUIRE(state.chats[1].id == "chat-b");
    REQUIRE(state.chats[0].messages.size() == 1);
    REQUIRE(state.chats[0].messages[0].text == "A3");
    REQUIRE(state.chats[1].messages.size() == 2);
    REQUIRE(state.chats[1].messages[0].text == "B1");
    CHECK(state.selectedChatId == "chat-a");

    std::filesystem::remove_all(tempDir);
}

TEST_CASE("Store can reorder chats and update selection without message churn") {
    std::filesystem::path const tempDir = uniqueTempDir("order");
    std::filesystem::create_directories(tempDir);

    lambda::Store store(tempDir);
    store.upsertChatThreadMeta("chat-a", "Chat A", 100, "/tmp/a.gguf", "A", 0);
    store.upsertChatThreadMeta("chat-b", "Chat B", 100, "/tmp/b.gguf", "B", 1);
    store.replaceChatMessagesForThread("chat-a", {makeUserMessage("A1")});
    store.replaceChatMessagesForThread("chat-b", {makeUserMessage("B1")});

    store.replaceChatOrder({"chat-b", "chat-a"});
    store.updateSelectedChatId("chat-b");

    lambda::PersistedChatState const state = store.loadPersistedChatState();
    REQUIRE(state.chats.size() == 2);
    CHECK(state.chats[0].id == "chat-b");
    CHECK(state.chats[1].id == "chat-a");
    CHECK(state.chats[0].messages.size() == 1);
    CHECK(state.chats[0].messages[0].text == "B1");
    CHECK(state.chats[1].messages.size() == 1);
    CHECK(state.chats[1].messages[0].text == "A1");
    CHECK(state.selectedChatId == "chat-b");

    std::filesystem::remove_all(tempDir);
}

TEST_CASE("Store deletes one chat thread without affecting others") {
    std::filesystem::path const tempDir = uniqueTempDir("delete");
    std::filesystem::create_directories(tempDir);

    lambda::Store store(tempDir);
    store.upsertChatThreadMeta("chat-a", "Chat A", 100, "/tmp/a.gguf", "A", 0);
    store.upsertChatThreadMeta("chat-b", "Chat B", 100, "/tmp/b.gguf", "B", 1);
    store.replaceChatMessagesForThread("chat-a", {makeUserMessage("A1"), makeUserMessage("A2")});
    store.replaceChatMessagesForThread("chat-b", {makeUserMessage("B1")});
    store.updateSelectedChatId("chat-b");

    store.deleteChatThread("chat-a");
    store.replaceChatOrder({"chat-b"});

    lambda::PersistedChatState const state = store.loadPersistedChatState();
    REQUIRE(state.chats.size() == 1);
    CHECK(state.chats[0].id == "chat-b");
    CHECK(state.chats[0].messages.size() == 1);
    CHECK(state.chats[0].messages[0].text == "B1");
    CHECK(state.selectedChatId == "chat-b");

    std::filesystem::remove_all(tempDir);
}

TEST_CASE("Store persists generation stats for reasoning messages") {
    std::filesystem::path const tempDir = uniqueTempDir("reasoning-stats");
    std::filesystem::create_directories(tempDir);

    lambda::Store store(tempDir);
    store.upsertChatThreadMeta("chat-a", "Chat A", 100, "/tmp/a.gguf", "A", 0);

    lambda::ChatMessage reasoning {
        .role = lambda::ChatRole::Reasoning,
        .text = "I should think first",
        .collapsed = true,
        .generationStats = makeGenerationStats("A", 11),
    };
    lambda::ChatMessage assistant {
        .role = lambda::ChatRole::Assistant,
        .text = "Here is the answer",
        .generationStats = makeGenerationStats("A", 33),
    };

    store.replaceChatMessagesForThread("chat-a", {makeUserMessage("Q"), reasoning, assistant});

    lambda::PersistedChatState const state = store.loadPersistedChatState();
    REQUIRE(state.chats.size() == 1);
    REQUIRE(state.chats[0].messages.size() == 3);
    REQUIRE(state.chats[0].messages[1].role == lambda::ChatRole::Reasoning);
    REQUIRE(state.chats[0].messages[1].generationStats.has_value());
    CHECK(state.chats[0].messages[1].generationStats->completionTokens == 11);
    REQUIRE(state.chats[0].messages[2].role == lambda::ChatRole::Assistant);
    REQUIRE(state.chats[0].messages[2].generationStats.has_value());
    CHECK(state.chats[0].messages[2].generationStats->completionTokens == 33);

    std::filesystem::remove_all(tempDir);
}
