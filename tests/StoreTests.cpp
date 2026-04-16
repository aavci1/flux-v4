#include <doctest/doctest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include <sqlite3.h>

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

lambda::ChatMessage makeToolMessage(
    std::string toolCallId,
    std::string toolName,
    std::string text,
    lambda::ToolMessageState toolState
) {
    return lambda::ChatMessage {
        .role = lambda::ChatRole::Tool,
        .text = std::move(text),
        .toolCallId = std::move(toolCallId),
        .toolName = std::move(toolName),
        .toolState = toolState,
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

lambda_studio_backend::GenerationParams makeGenerationDefaults(float temp, std::int32_t topK) {
    return lambda_studio_backend::GenerationParams {
        .topK = topK,
        .temp = temp,
    };
}

void execSql(sqlite3 *db, char const *sql) {
    char *error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        std::string message = error != nullptr ? error : "sqlite error";
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}

} // namespace

TEST_CASE("Store incremental chat persistence updates only targeted thread") {
    std::filesystem::path const tempDir = uniqueTempDir("thread");
    std::filesystem::create_directories(tempDir);

    lambda::Store store(tempDir);
    store.upsertChatThreadMeta("chat-a", "Chat A", 100, "/tmp/a.gguf", "A", "", 0, 0, 0);
    store.upsertChatThreadMeta("chat-b", "Chat B", 100, "/tmp/b.gguf", "B", "", 0, 0, 1);
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
    store.upsertChatThreadMeta("chat-a", "Chat A", 100, "/tmp/a.gguf", "A", "", 0, 0, 0);
    store.upsertChatThreadMeta("chat-b", "Chat B", 100, "/tmp/b.gguf", "B", "", 0, 0, 1);
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
    store.upsertChatThreadMeta("chat-a", "Chat A", 100, "/tmp/a.gguf", "A", "", 0, 0, 0);
    store.upsertChatThreadMeta("chat-b", "Chat B", 100, "/tmp/b.gguf", "B", "", 0, 0, 1);
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
    store.upsertChatThreadMeta("chat-a", "Chat A", 100, "/tmp/a.gguf", "A", "", 0, 0, 0);

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

TEST_CASE("Store persists hidden chat summary metadata") {
    std::filesystem::path const tempDir = uniqueTempDir("summary-meta");
    std::filesystem::create_directories(tempDir);

    lambda::Store store(tempDir);
    store.upsertChatThreadMeta(
        "chat-a",
        "Chat A",
        100,
        "/tmp/a.gguf",
        "A",
        "Summarized earlier turns",
        3,
        456,
        0
    );
    store.replaceChatMessagesForThread("chat-a", {makeUserMessage("Q"), makeUserMessage("A")});

    lambda::PersistedChatState const state = store.loadPersistedChatState();
    REQUIRE(state.chats.size() == 1);
    CHECK(state.chats[0].summaryText == "Summarized earlier turns");
    CHECK(state.chats[0].summaryMessageCount == 3);
    CHECK(state.chats[0].summaryUpdatedAtUnixMs == 456);

    std::filesystem::remove_all(tempDir);
}

TEST_CASE("Store persists per-chat generation defaults without rewriting other chats") {
    std::filesystem::path const tempDir = uniqueTempDir("chat-generation-defaults");
    std::filesystem::create_directories(tempDir);

    lambda::Store store(tempDir);
    store.upsertChatThreadMeta("chat-a", "Chat A", 100, "/tmp/a.gguf", "A", "", 0, 0, 0);
    store.upsertChatThreadMeta("chat-b", "Chat B", 100, "/tmp/b.gguf", "B", "", 0, 0, 1);
    store.replaceChatMessagesForThread("chat-a", {makeUserMessage("A1")});
    store.replaceChatMessagesForThread("chat-b", {makeUserMessage("B1")});

    store.updateChatThreadGenerationDefaults("chat-a", makeGenerationDefaults(0.55f, 77));

    lambda::PersistedChatState const state = store.loadPersistedChatState();
    REQUIRE(state.chats.size() == 2);
    REQUIRE(state.chats[0].id == "chat-a");
    REQUIRE(state.chats[0].generationDefaults.has_value());
    CHECK(doctest::Approx(state.chats[0].generationDefaults->temp).epsilon(0.001f) == 0.55f);
    CHECK(state.chats[0].generationDefaults->topK == 77);
    CHECK(state.chats[0].messages.size() == 1);
    CHECK(state.chats[0].messages[0].text == "A1");
    REQUIRE(state.chats[1].id == "chat-b");
    CHECK(!state.chats[1].generationDefaults.has_value());
    CHECK(state.chats[1].messages.size() == 1);
    CHECK(state.chats[1].messages[0].text == "B1");

    std::filesystem::remove_all(tempDir);
}

TEST_CASE("Store persists engine configuration defaults") {
    std::filesystem::path const tempDir = uniqueTempDir("engine-defaults");
    std::filesystem::create_directories(tempDir);

    lambda::Store store(tempDir);
    lambda_studio_backend::EngineConfigDefaults defaults;
    defaults.loadDefaults.modelPath = "/tmp/model.gguf";
    defaults.loadDefaults.nGpuLayers = 21;
    defaults.sessionDefaults.nCtx = 8192;
    defaults.sessionDefaults.toolConfig.enabled = true;
    defaults.sessionDefaults.toolConfig.workspaceRoot = "/tmp/workspace";
    defaults.sessionDefaults.toolConfig.maxToolCalls = 5;
    defaults.sessionDefaults.toolConfig.enabledToolNames = {"read_file", "grep_search"};
    defaults.generationDefaults.temp = 0.42f;
    defaults.generationDefaults.topK = 17;

    store.saveEngineConfigDefaults(defaults);
    auto loaded = store.loadEngineConfigDefaults();
    REQUIRE(loaded.has_value());
    CHECK(loaded->loadDefaults.modelPath == "/tmp/model.gguf");
    CHECK(loaded->loadDefaults.nGpuLayers == 21);
    CHECK(loaded->sessionDefaults.nCtx == 8192);
    CHECK(loaded->sessionDefaults.toolConfig.enabled);
    CHECK(
        std::filesystem::weakly_canonical(loaded->sessionDefaults.toolConfig.workspaceRoot).string() ==
        std::filesystem::weakly_canonical("/tmp/workspace").string()
    );
    CHECK(loaded->sessionDefaults.toolConfig.maxToolCalls == 5);
    REQUIRE(loaded->sessionDefaults.toolConfig.enabledToolNames.size() == 2);
    CHECK(loaded->sessionDefaults.toolConfig.enabledToolNames[0] == "read_file");
    CHECK(loaded->sessionDefaults.toolConfig.enabledToolNames[1] == "grep_search");
    CHECK(doctest::Approx(loaded->generationDefaults.temp).epsilon(0.001f) == 0.42f);
    CHECK(loaded->generationDefaults.topK == 17);

    std::filesystem::remove_all(tempDir);
}

TEST_CASE("Store round-trips assistant tool calls and tool messages") {
    std::filesystem::path const tempDir = uniqueTempDir("tool-roundtrip");
    std::filesystem::create_directories(tempDir);

    lambda::Store store(tempDir);
    store.upsertChatThreadMeta("chat-a", "Chat A", 100, "/tmp/a.gguf", "A", "", 0, 0, 0);

    lambda::ChatMessage assistant {
        .role = lambda::ChatRole::Assistant,
        .toolCalls = {
            lambda::ChatToolCall {
                .id = "call_1",
                .name = "read_file",
                .arguments = "{\"path\":\"README.md\"}",
            },
            lambda::ChatToolCall {
                .id = "call_2",
                .name = "grep_search",
                .arguments = "{\"path\":\".\",\"pattern\":\"TODO\"}",
            },
        },
    };
    lambda::ChatMessage tool = makeToolMessage(
        "call_1",
        "read_file",
        "{\"ok\":true,\"tool\":\"read_file\"}",
        lambda::ToolMessageState::Completed
    );

    store.replaceChatMessagesForThread("chat-a", {makeUserMessage("Q"), assistant, tool});

    lambda::PersistedChatState const state = store.loadPersistedChatState();
    REQUIRE(state.chats.size() == 1);
    REQUIRE(state.chats[0].messages.size() == 3);
    REQUIRE(state.chats[0].messages[1].role == lambda::ChatRole::Assistant);
    REQUIRE(state.chats[0].messages[1].toolCalls.size() == 2);
    CHECK(state.chats[0].messages[1].toolCalls[0].id == "call_1");
    CHECK(state.chats[0].messages[1].toolCalls[0].name == "read_file");
    CHECK(state.chats[0].messages[1].toolCalls[1].name == "grep_search");
    REQUIRE(state.chats[0].messages[2].role == lambda::ChatRole::Tool);
    CHECK(state.chats[0].messages[2].toolCallId == "call_1");
    CHECK(state.chats[0].messages[2].toolName == "read_file");
    CHECK(state.chats[0].messages[2].toolState == lambda::ToolMessageState::Completed);
    CHECK(state.chats[0].messages[2].text == "{\"ok\":true,\"tool\":\"read_file\"}");

    std::filesystem::remove_all(tempDir);
}

TEST_CASE("Store migrates v6 chat threads to v7 summary metadata") {
    std::filesystem::path const tempDir = uniqueTempDir("migration-v7");
    std::filesystem::create_directories(tempDir);
    std::filesystem::path const dbPath = tempDir / "model_catalog.sqlite3";

    sqlite3 *db = nullptr;
    REQUIRE(sqlite3_open_v2(dbPath.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    execSql(
        db,
        "CREATE TABLE app_preferences (pref_key TEXT PRIMARY KEY, value_text TEXT NOT NULL DEFAULT '');"
        "CREATE TABLE chat_threads ("
        "  chat_id TEXT PRIMARY KEY,"
        "  title TEXT NOT NULL,"
        "  updated_at_unix_ms INTEGER NOT NULL DEFAULT 0,"
        "  model_path TEXT NOT NULL DEFAULT '',"
        "  model_name TEXT NOT NULL DEFAULT '',"
        "  sort_order INTEGER NOT NULL"
        ");"
        "CREATE TABLE chat_messages ("
        "  chat_id TEXT NOT NULL,"
        "  message_order INTEGER NOT NULL,"
        "  role TEXT NOT NULL,"
        "  text TEXT NOT NULL DEFAULT '',"
        "  started_at_nanos INTEGER NOT NULL DEFAULT 0,"
        "  finished_at_nanos INTEGER NOT NULL DEFAULT 0,"
        "  collapsed INTEGER NOT NULL DEFAULT 0,"
        "  PRIMARY KEY (chat_id, message_order)"
        ");"
        "CREATE TABLE chat_message_stats ("
        "  chat_id TEXT NOT NULL,"
        "  message_order INTEGER NOT NULL,"
        "  model_path TEXT NOT NULL DEFAULT '',"
        "  model_name TEXT NOT NULL DEFAULT '',"
        "  prompt_tokens INTEGER NOT NULL DEFAULT 0,"
        "  completion_tokens INTEGER NOT NULL DEFAULT 0,"
        "  started_at_unix_ms INTEGER NOT NULL DEFAULT 0,"
        "  first_token_at_unix_ms INTEGER NOT NULL DEFAULT 0,"
        "  finished_at_unix_ms INTEGER NOT NULL DEFAULT 0,"
        "  tokens_per_second REAL NOT NULL DEFAULT 0,"
        "  status TEXT NOT NULL DEFAULT '',"
        "  error_text TEXT NOT NULL DEFAULT '',"
        "  temp REAL NOT NULL DEFAULT 0,"
        "  top_p REAL NOT NULL DEFAULT 0,"
        "  top_k INTEGER NOT NULL DEFAULT 0,"
        "  max_tokens INTEGER NOT NULL DEFAULT 0,"
        "  PRIMARY KEY (chat_id, message_order)"
        ");"
        "INSERT INTO chat_threads (chat_id, title, updated_at_unix_ms, model_path, model_name, sort_order) "
        "VALUES ('chat-a', 'Chat A', 100, '/tmp/a.gguf', 'A', 0);"
        "INSERT INTO chat_messages (chat_id, message_order, role, text, started_at_nanos, finished_at_nanos, collapsed) "
        "VALUES ('chat-a', 0, 'user', 'hello', 0, 0, 0);"
        "PRAGMA user_version=6;"
    );
    sqlite3_close(db);

    lambda::Store store(tempDir);
    lambda::PersistedChatState state = store.loadPersistedChatState();
    REQUIRE(state.chats.size() == 1);
    CHECK(state.chats[0].messages.size() == 1);
    CHECK(state.chats[0].summaryText.empty());
    CHECK(state.chats[0].summaryMessageCount == 0);

    store.upsertChatThreadMeta(
        "chat-a",
        "Chat A",
        101,
        "/tmp/a.gguf",
        "A",
        "Migrated summary",
        2,
        999,
        0
    );

    state = store.loadPersistedChatState();
    REQUIRE(state.chats.size() == 1);
    CHECK(state.chats[0].summaryText == "Migrated summary");
    CHECK(state.chats[0].summaryMessageCount == 2);
    CHECK(state.chats[0].summaryUpdatedAtUnixMs == 999);
    CHECK(state.chats[0].messages.size() == 1);
    CHECK(state.chats[0].messages[0].text == "hello");

    std::filesystem::remove_all(tempDir);
}

TEST_CASE("Store can delete download jobs individually and by artifact") {
    std::filesystem::path const tempDir = uniqueTempDir("downloads");
    std::filesystem::create_directories(tempDir);

    lambda::Store store(tempDir);
    lambda::DownloadJob failedA {
        .id = "job-a",
        .repoId = "repo",
        .filePath = "model.gguf",
        .error = "network",
        .startedAtUnixMs = 100,
        .finishedAtUnixMs = 110,
        .status = lambda::DownloadJobStatus::Failed,
    };
    lambda::DownloadJob failedB {
        .id = "job-b",
        .repoId = "repo",
        .filePath = "model.gguf",
        .error = "network",
        .startedAtUnixMs = 200,
        .finishedAtUnixMs = 210,
        .status = lambda::DownloadJobStatus::Failed,
    };
    lambda::DownloadJob failedC {
        .id = "job-c",
        .repoId = "repo",
        .filePath = "other.gguf",
        .error = "network",
        .startedAtUnixMs = 300,
        .finishedAtUnixMs = 310,
        .status = lambda::DownloadJobStatus::Failed,
    };

    store.startDownloadJob(failedA);
    store.startDownloadJob(failedB);
    store.startDownloadJob(failedC);

    store.deleteDownloadJob("job-a");
    auto jobs = store.loadRecentDownloadJobs();
    REQUIRE(jobs.size() == 2);
    CHECK(jobs[0].id == "job-c");
    CHECK(jobs[1].id == "job-b");

    store.deleteDownloadJobsForArtifact("repo", "model.gguf");
    jobs = store.loadRecentDownloadJobs();
    REQUIRE(jobs.size() == 1);
    CHECK(jobs[0].id == "job-c");

    std::filesystem::remove_all(tempDir);
}
