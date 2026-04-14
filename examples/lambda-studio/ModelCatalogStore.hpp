#pragma once

#include "BackendInterfaces.hpp"

#define JSON_ASSERT(x) ((x) ? static_cast<void>(0) : std::abort())
#include <nlohmann/json.hpp>
#include <sqlite3.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lambda {

class ModelCatalogStore : public IModelCatalogStore {
  public:
    ModelCatalogStore() : ModelCatalogStore(defaultDataDirectory()) {}

    explicit ModelCatalogStore(std::filesystem::path dataDirectory)
        : dataDirectory_(std::move(dataDirectory)) {
        tryOpen(dataDirectory_);
    }

    ~ModelCatalogStore() {
        if (db_ != nullptr) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }

    ModelCatalogStore(ModelCatalogStore const &) = delete;
    ModelCatalogStore &operator=(ModelCatalogStore const &) = delete;

    std::filesystem::path databasePath() const override {
        return dataDirectory_ / "model_catalog.sqlite3";
    }

    void replaceSearchSnapshot(
        std::string const &query,
        std::vector<RemoteModel> const &models,
        std::string const &rawJson
    ) override {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            beginTransaction();

            Statement deleteSearch(db_, "DELETE FROM search_results WHERE query = ?1;");
            bindText(deleteSearch.stmt, 1, query);
            stepDone(deleteSearch.stmt);

            Statement upsertPayload(
                db_,
                "INSERT INTO search_payloads (query, raw_json, fetched_at_unix_ms) VALUES (?1, ?2, ?3)"
                " ON CONFLICT(query) DO UPDATE SET raw_json=excluded.raw_json, fetched_at_unix_ms=excluded.fetched_at_unix_ms;"
            );
            bindText(upsertPayload.stmt, 1, query);
            bindText(upsertPayload.stmt, 2, rawJson);
            bindInt64(upsertPayload.stmt, 3, currentUnixMillis());
            stepDone(upsertPayload.stmt);

            Statement upsertRepo(
                db_,
                "INSERT INTO model_repos ("
                "  repo_id, author, library_name, pipeline_tag, created_at, last_modified, tags,"
                "  downloads, downloads_all_time, likes, used_storage, gated, is_private, disabled"
                ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14)"
                " ON CONFLICT(repo_id) DO UPDATE SET "
                "  author=excluded.author,"
                "  library_name=excluded.library_name,"
                "  pipeline_tag=excluded.pipeline_tag,"
                "  created_at=excluded.created_at,"
                "  last_modified=excluded.last_modified,"
                "  tags=excluded.tags,"
                "  downloads=excluded.downloads,"
                "  downloads_all_time=excluded.downloads_all_time,"
                "  likes=excluded.likes,"
                "  used_storage=excluded.used_storage,"
                "  gated=excluded.gated,"
                "  is_private=excluded.is_private,"
                "  disabled=excluded.disabled;"
            );
            Statement insertSearch(
                db_,
                "INSERT INTO search_results (query, repo_id, rank_index) VALUES (?1, ?2, ?3);"
            );

            for (std::size_t i = 0; i < models.size(); ++i) {
                RemoteModel const &model = models[i];
                sqlite3_reset(upsertRepo.stmt);
                sqlite3_clear_bindings(upsertRepo.stmt);
                bindText(upsertRepo.stmt, 1, model.id);
                bindText(upsertRepo.stmt, 2, model.author);
                bindText(upsertRepo.stmt, 3, model.libraryName);
                bindText(upsertRepo.stmt, 4, model.pipelineTag);
                bindText(upsertRepo.stmt, 5, model.createdAt);
                bindText(upsertRepo.stmt, 6, model.lastModified);
                bindText(upsertRepo.stmt, 7, joinTags(model.tags));
                bindInt64(upsertRepo.stmt, 8, model.downloads);
                bindInt64(upsertRepo.stmt, 9, model.downloadsAllTime);
                bindInt64(upsertRepo.stmt, 10, model.likes);
                bindInt64(upsertRepo.stmt, 11, model.usedStorage);
                bindBool(upsertRepo.stmt, 12, model.gated);
                bindBool(upsertRepo.stmt, 13, model.isPrivate);
                bindBool(upsertRepo.stmt, 14, model.disabled);
                stepDone(upsertRepo.stmt);

                sqlite3_reset(insertSearch.stmt);
                sqlite3_clear_bindings(insertSearch.stmt);
                bindText(insertSearch.stmt, 1, query);
                bindText(insertSearch.stmt, 2, model.id);
                bindInt64(insertSearch.stmt, 3, static_cast<std::int64_t>(i));
                stepDone(insertSearch.stmt);
            }

            commitTransaction();
        } catch (...) {
            rollbackTransaction();
            throw;
        }
    }

    std::vector<RemoteModel> loadSearchResults(std::string const &query) override {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement stmt(
            db_,
            "SELECT "
            "  r.repo_id, r.author, r.library_name, r.pipeline_tag, r.created_at, r.last_modified, r.tags,"
            "  r.downloads, r.downloads_all_time, r.likes, r.used_storage, r.gated, r.is_private, r.disabled "
            "FROM search_results s "
            "JOIN model_repos r ON r.repo_id = s.repo_id "
            "WHERE s.query = ?1 "
            "ORDER BY s.rank_index ASC;"
        );
        bindText(stmt.stmt, 1, query);

        std::vector<RemoteModel> models;
        while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
            models.push_back(readRemoteModelRow(stmt.stmt));
        }
        if (models.empty()) {
            models = loadSearchResultsFromPayload(query);
        }
        return models;
    }

    std::vector<RemoteModel> searchCatalogModels(
        std::string const &query,
        std::string const &author,
        RemoteModelSort sort,
        RemoteModelVisibilityFilter visibility,
        std::size_t limit = 20
    ) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string orderBy = "r.downloads DESC, r.likes DESC, r.repo_id ASC";
        if (sort == RemoteModelSort::Likes) {
            orderBy = "r.likes DESC, r.downloads DESC, r.repo_id ASC";
        } else if (sort == RemoteModelSort::Updated) {
            orderBy = "r.last_modified DESC, r.downloads DESC, r.repo_id ASC";
        }

        std::string sql =
            "SELECT "
            "  r.repo_id, r.author, r.library_name, r.pipeline_tag, r.created_at, r.last_modified, r.tags,"
            "  r.downloads, r.downloads_all_time, r.likes, r.used_storage, r.gated, r.is_private, r.disabled "
            "FROM model_repos r "
            "LEFT JOIN repo_details d ON d.repo_id = r.repo_id "
            "WHERE (?1 = '' OR "
            "       r.repo_id LIKE ?2 OR "
            "       r.author LIKE ?2 OR "
            "       r.tags LIKE ?2 OR "
            "       COALESCE(d.summary, '') LIKE ?2) "
            "  AND (?3 = '' OR r.author LIKE ?4) "
            "  AND (?5 = 'all' OR (?5 = 'public' AND r.gated = 0 AND r.is_private = 0 AND r.disabled = 0) "
            "       OR (?5 = 'gated' AND r.gated = 1)) "
            "ORDER BY " + orderBy + " "
            "LIMIT ?6;";

        Statement stmt(db_, sql.c_str());
        std::string const queryPattern = "%" + query + "%";
        std::string const authorPattern = "%" + author + "%";
        bindText(stmt.stmt, 1, query);
        bindText(stmt.stmt, 2, queryPattern);
        bindText(stmt.stmt, 3, author);
        bindText(stmt.stmt, 4, authorPattern);
        bindText(
            stmt.stmt,
            5,
            visibility == RemoteModelVisibilityFilter::PublicOnly ? "public" :
            visibility == RemoteModelVisibilityFilter::GatedOnly ? "gated" :
                                                                   "all"
        );
        bindInt64(stmt.stmt, 6, static_cast<std::int64_t>(limit));

        std::vector<RemoteModel> models;
        while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
            models.push_back(readRemoteModelRow(stmt.stmt));
        }
        return models;
    }

    void replaceRepoFilesSnapshot(
        std::string const &repoId,
        std::vector<RemoteModelFile> const &files,
        std::string const &rawJson
    ) override {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            beginTransaction();

            Statement deleteFiles(db_, "DELETE FROM repo_files WHERE repo_id = ?1;");
            bindText(deleteFiles.stmt, 1, repoId);
            stepDone(deleteFiles.stmt);

            Statement upsertPayload(
                db_,
                "INSERT INTO repo_file_payloads (repo_id, raw_json, fetched_at_unix_ms) VALUES (?1, ?2, ?3)"
                " ON CONFLICT(repo_id) DO UPDATE SET raw_json=excluded.raw_json, fetched_at_unix_ms=excluded.fetched_at_unix_ms;"
            );
            bindText(upsertPayload.stmt, 1, repoId);
            bindText(upsertPayload.stmt, 2, rawJson);
            bindInt64(upsertPayload.stmt, 3, currentUnixMillis());
            stepDone(upsertPayload.stmt);

            Statement insertFile(
                db_,
                "INSERT INTO repo_files (repo_id, path, local_path, size_bytes, cached) "
                "VALUES (?1, ?2, ?3, ?4, ?5);"
            );
            for (RemoteModelFile const &file : files) {
                sqlite3_reset(insertFile.stmt);
                sqlite3_clear_bindings(insertFile.stmt);
                bindText(insertFile.stmt, 1, repoId);
                bindText(insertFile.stmt, 2, file.path);
                bindText(insertFile.stmt, 3, file.localPath);
                bindInt64(insertFile.stmt, 4, static_cast<std::int64_t>(file.sizeBytes));
                bindBool(insertFile.stmt, 5, file.cached);
                stepDone(insertFile.stmt);
            }

            commitTransaction();
        } catch (...) {
            rollbackTransaction();
            throw;
        }
    }

    std::vector<RemoteModelFile> loadRepoFiles(std::string const &repoId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement stmt(
            db_,
            "SELECT repo_id, path, local_path, size_bytes, cached "
            "FROM repo_files "
            "WHERE repo_id = ?1 "
            "ORDER BY path ASC;"
        );
        bindText(stmt.stmt, 1, repoId);

        std::vector<RemoteModelFile> files;
        while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
            RemoteModelFile file;
            file.repoId = columnText(stmt.stmt, 0);
            file.path = columnText(stmt.stmt, 1);
            file.localPath = columnText(stmt.stmt, 2);
            file.sizeBytes = static_cast<std::size_t>(sqlite3_column_int64(stmt.stmt, 3));
            file.cached = sqlite3_column_int(stmt.stmt, 4) != 0;
            files.push_back(std::move(file));
        }
        if (files.empty()) {
            files = loadRepoFilesFromPayload(repoId);
        }
        return files;
    }

    void replaceRepoDetailSnapshot(RemoteRepoDetail const &detail, std::string const &rawJson) override {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement upsert(
            db_,
            "INSERT INTO repo_details ("
            "  repo_id, sha, license, summary, readme_text, raw_json, fetched_at_unix_ms"
            ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)"
            " ON CONFLICT(repo_id) DO UPDATE SET "
            "  sha=excluded.sha,"
            "  license=excluded.license,"
            "  summary=excluded.summary,"
            "  readme_text=excluded.readme_text,"
            "  raw_json=excluded.raw_json,"
            "  fetched_at_unix_ms=excluded.fetched_at_unix_ms;"
        );
        bindText(upsert.stmt, 1, detail.id);
        bindText(upsert.stmt, 2, detail.sha);
        bindText(upsert.stmt, 3, detail.license);
        bindText(upsert.stmt, 4, detail.summary);
        bindText(upsert.stmt, 5, detail.readme);
        bindText(upsert.stmt, 6, rawJson);
        bindInt64(upsert.stmt, 7, currentUnixMillis());
        stepDone(upsert.stmt);
    }

    std::optional<RemoteRepoDetail> loadRepoDetail(std::string const &repoId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement stmt(
            db_,
            "SELECT raw_json FROM repo_details WHERE repo_id = ?1;"
        );
        bindText(stmt.stmt, 1, repoId);
        if (sqlite3_step(stmt.stmt) != SQLITE_ROW) {
            return std::nullopt;
        }
        auto detail = parseRepoDetailPayload(columnText(stmt.stmt, 0));
        if (!detail.has_value()) {
            return std::nullopt;
        }
        return detail;
    }

    void replaceLocalModelInstances(std::vector<LocalModel> const &models) override {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            beginTransaction();
            execute("DELETE FROM local_model_instances;");

            Statement insert(
                db_,
                "INSERT INTO local_model_instances (path, name, repo_id, tag, size_bytes, discovered_at_unix_ms) "
                "VALUES (?1, ?2, ?3, ?4, ?5, ?6);"
            );
            std::int64_t const now = currentUnixMillis();
            for (LocalModel const &model : models) {
                sqlite3_reset(insert.stmt);
                sqlite3_clear_bindings(insert.stmt);
                bindText(insert.stmt, 1, model.path);
                bindText(insert.stmt, 2, model.name);
                bindText(insert.stmt, 3, model.repo);
                bindText(insert.stmt, 4, model.tag);
                bindInt64(insert.stmt, 5, static_cast<std::int64_t>(model.sizeBytes));
                bindInt64(insert.stmt, 6, now);
                stepDone(insert.stmt);
            }

            commitTransaction();
        } catch (...) {
            rollbackTransaction();
            throw;
        }
    }

    std::vector<LocalModel> loadLocalModelInstances() override {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement stmt(
            db_,
            "SELECT path, name, repo_id, tag, size_bytes "
            "FROM local_model_instances "
            "ORDER BY name ASC, path ASC;"
        );

        std::vector<LocalModel> models;
        while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
            LocalModel model;
            model.path = columnText(stmt.stmt, 0);
            model.name = columnText(stmt.stmt, 1);
            model.repo = columnText(stmt.stmt, 2);
            model.tag = columnText(stmt.stmt, 3);
            model.sizeBytes = static_cast<std::size_t>(sqlite3_column_int64(stmt.stmt, 4));
            models.push_back(std::move(model));
        }
        return models;
    }

    void startDownloadJob(DownloadJob const &job) override {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement upsert(
            db_,
            "INSERT INTO download_jobs ("
            "  job_id, repo_id, file_path, local_path, error_text, status, started_at_unix_ms, finished_at_unix_ms"
            ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)"
            " ON CONFLICT(job_id) DO UPDATE SET "
            "  repo_id=excluded.repo_id,"
            "  file_path=excluded.file_path,"
            "  local_path=excluded.local_path,"
            "  error_text=excluded.error_text,"
            "  status=excluded.status,"
            "  started_at_unix_ms=excluded.started_at_unix_ms,"
            "  finished_at_unix_ms=excluded.finished_at_unix_ms;"
        );
        bindText(upsert.stmt, 1, job.id);
        bindText(upsert.stmt, 2, job.repoId);
        bindText(upsert.stmt, 3, job.filePath);
        bindText(upsert.stmt, 4, job.localPath);
        bindText(upsert.stmt, 5, job.error);
        bindText(upsert.stmt, 6, downloadJobStatusStorage(job.status));
        bindInt64(upsert.stmt, 7, job.startedAtUnixMs);
        bindInt64(upsert.stmt, 8, job.finishedAtUnixMs);
        stepDone(upsert.stmt);
    }

    void finishDownloadJob(
        std::string const &jobId,
        std::string const &localPath,
        std::int64_t finishedAtUnixMs
    ) override {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement update(
            db_,
            "UPDATE download_jobs "
            "SET local_path = ?2, error_text = '', status = 'completed', finished_at_unix_ms = ?3 "
            "WHERE job_id = ?1;"
        );
        bindText(update.stmt, 1, jobId);
        bindText(update.stmt, 2, localPath);
        bindInt64(update.stmt, 3, finishedAtUnixMs);
        stepDone(update.stmt);
    }

    void failDownloadJob(
        std::string const &jobId,
        std::string const &errorText,
        std::int64_t finishedAtUnixMs
    ) override {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement update(
            db_,
            "UPDATE download_jobs "
            "SET error_text = ?2, status = 'failed', finished_at_unix_ms = ?3 "
            "WHERE job_id = ?1;"
        );
        bindText(update.stmt, 1, jobId);
        bindText(update.stmt, 2, errorText);
        bindInt64(update.stmt, 3, finishedAtUnixMs);
        stepDone(update.stmt);
    }

    std::vector<DownloadJob> loadRecentDownloadJobs(std::size_t limit = 12) override {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement stmt(
            db_,
            "SELECT job_id, repo_id, file_path, local_path, error_text, status, started_at_unix_ms, finished_at_unix_ms "
            "FROM download_jobs "
            "ORDER BY started_at_unix_ms DESC "
            "LIMIT ?1;"
        );
        bindInt64(stmt.stmt, 1, static_cast<std::int64_t>(limit));

        std::vector<DownloadJob> jobs;
        while (sqlite3_step(stmt.stmt) == SQLITE_ROW) {
            DownloadJob job;
            job.id = columnText(stmt.stmt, 0);
            job.repoId = columnText(stmt.stmt, 1);
            job.filePath = columnText(stmt.stmt, 2);
            job.localPath = columnText(stmt.stmt, 3);
            job.error = columnText(stmt.stmt, 4);
            job.status = downloadJobStatusFromStorage(columnText(stmt.stmt, 5));
            job.startedAtUnixMs = sqlite3_column_int64(stmt.stmt, 6);
            job.finishedAtUnixMs = sqlite3_column_int64(stmt.stmt, 7);
            jobs.push_back(std::move(job));
        }
        return jobs;
    }

    void markRunningDownloadJobsInterrupted(std::int64_t finishedAtUnixMs) override {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement update(
            db_,
            "UPDATE download_jobs "
            "SET status = 'failed', error_text = 'Interrupted', finished_at_unix_ms = ?1 "
            "WHERE status = 'running';"
        );
        bindInt64(update.stmt, 1, finishedAtUnixMs);
        stepDone(update.stmt);
    }

    void upsertChatThreadMeta(
        std::string const &chatId,
        std::string const &title,
        std::int64_t updatedAtUnixMs,
        std::string const &modelPath,
        std::string const &modelName,
        std::int64_t sortOrder
    ) override {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement upsert(
            db_,
            "INSERT INTO chat_threads (chat_id, title, updated_at_unix_ms, model_path, model_name, sort_order) "
            "VALUES (?1, ?2, ?3, ?4, ?5, ?6) "
            "ON CONFLICT(chat_id) DO UPDATE SET "
            "  title=excluded.title,"
            "  updated_at_unix_ms=excluded.updated_at_unix_ms,"
            "  model_path=excluded.model_path,"
            "  model_name=excluded.model_name,"
            "  sort_order=excluded.sort_order;"
        );
        bindText(upsert.stmt, 1, chatId);
        bindText(upsert.stmt, 2, title);
        bindInt64(upsert.stmt, 3, updatedAtUnixMs);
        bindText(upsert.stmt, 4, modelPath);
        bindText(upsert.stmt, 5, modelName);
        bindInt64(upsert.stmt, 6, sortOrder);
        stepDone(upsert.stmt);
    }

    void replaceChatMessagesForThread(
        std::string const &chatId,
        std::vector<ChatMessage> const &messages
    ) override {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            beginTransaction();

            Statement deleteStats(
                db_,
                "DELETE FROM chat_message_stats WHERE chat_id = ?1;"
            );
            bindText(deleteStats.stmt, 1, chatId);
            stepDone(deleteStats.stmt);

            Statement deleteMessages(
                db_,
                "DELETE FROM chat_messages WHERE chat_id = ?1;"
            );
            bindText(deleteMessages.stmt, 1, chatId);
            stepDone(deleteMessages.stmt);

            Statement insertMessage(
                db_,
                "INSERT INTO chat_messages ("
                "  chat_id, message_order, role, text, started_at_nanos, finished_at_nanos, collapsed"
                ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7);"
            );
            Statement insertMessageStats(
                db_,
                "INSERT INTO chat_message_stats ("
                "  chat_id, message_order, model_path, model_name, prompt_tokens, completion_tokens,"
                "  started_at_unix_ms, first_token_at_unix_ms, finished_at_unix_ms, tokens_per_second,"
                "  status, error_text, temp, top_p, top_k, max_tokens"
                ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16);"
            );

            for (std::size_t messageIndex = 0; messageIndex < messages.size(); ++messageIndex) {
                ChatMessage const &message = messages[messageIndex];
                sqlite3_reset(insertMessage.stmt);
                sqlite3_clear_bindings(insertMessage.stmt);
                bindText(insertMessage.stmt, 1, chatId);
                bindInt64(insertMessage.stmt, 2, static_cast<std::int64_t>(messageIndex));
                bindText(insertMessage.stmt, 3, chatRoleStorage(message.role));
                bindText(insertMessage.stmt, 4, message.text);
                bindInt64(insertMessage.stmt, 5, message.startedAtNanos);
                bindInt64(insertMessage.stmt, 6, message.finishedAtNanos);
                bindBool(insertMessage.stmt, 7, message.collapsed);
                stepDone(insertMessage.stmt);

                if (!message.generationStats.has_value()) {
                    continue;
                }

                MessageGenerationStats const &stats = *message.generationStats;
                sqlite3_reset(insertMessageStats.stmt);
                sqlite3_clear_bindings(insertMessageStats.stmt);
                bindText(insertMessageStats.stmt, 1, chatId);
                bindInt64(insertMessageStats.stmt, 2, static_cast<std::int64_t>(messageIndex));
                bindText(insertMessageStats.stmt, 3, stats.modelPath);
                bindText(insertMessageStats.stmt, 4, stats.modelName);
                bindInt64(insertMessageStats.stmt, 5, stats.promptTokens);
                bindInt64(insertMessageStats.stmt, 6, stats.completionTokens);
                bindInt64(insertMessageStats.stmt, 7, stats.startedAtUnixMs);
                bindInt64(insertMessageStats.stmt, 8, stats.firstTokenAtUnixMs);
                bindInt64(insertMessageStats.stmt, 9, stats.finishedAtUnixMs);
                bindDouble(insertMessageStats.stmt, 10, stats.tokensPerSecond);
                bindText(insertMessageStats.stmt, 11, stats.status);
                bindText(insertMessageStats.stmt, 12, stats.errorText);
                bindDouble(insertMessageStats.stmt, 13, static_cast<double>(stats.temp));
                bindDouble(insertMessageStats.stmt, 14, static_cast<double>(stats.topP));
                bindInt64(insertMessageStats.stmt, 15, stats.topK);
                bindInt64(insertMessageStats.stmt, 16, stats.maxTokens);
                stepDone(insertMessageStats.stmt);
            }

            commitTransaction();
        } catch (...) {
            rollbackTransaction();
            throw;
        }
    }

    void deleteChatThread(std::string const &chatId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            beginTransaction();

            Statement deleteStats(
                db_,
                "DELETE FROM chat_message_stats WHERE chat_id = ?1;"
            );
            bindText(deleteStats.stmt, 1, chatId);
            stepDone(deleteStats.stmt);

            Statement deleteMessages(
                db_,
                "DELETE FROM chat_messages WHERE chat_id = ?1;"
            );
            bindText(deleteMessages.stmt, 1, chatId);
            stepDone(deleteMessages.stmt);

            Statement deleteThread(
                db_,
                "DELETE FROM chat_threads WHERE chat_id = ?1;"
            );
            bindText(deleteThread.stmt, 1, chatId);
            stepDone(deleteThread.stmt);

            commitTransaction();
        } catch (...) {
            rollbackTransaction();
            throw;
        }
    }

    void updateSelectedChatId(std::string const &selectedChatId) override {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement upsertPreference(
            db_,
            "INSERT INTO app_preferences (pref_key, value_text) VALUES (?1, ?2) "
            "ON CONFLICT(pref_key) DO UPDATE SET value_text=excluded.value_text;"
        );
        bindText(upsertPreference.stmt, 1, "selected_chat_id");
        bindText(upsertPreference.stmt, 2, selectedChatId);
        stepDone(upsertPreference.stmt);
    }

    void replaceChatOrder(std::vector<std::string> const &chatIds) override {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            beginTransaction();
            Statement update(
                db_,
                "UPDATE chat_threads SET sort_order = ?2 WHERE chat_id = ?1;"
            );
            for (std::size_t index = 0; index < chatIds.size(); ++index) {
                sqlite3_reset(update.stmt);
                sqlite3_clear_bindings(update.stmt);
                bindText(update.stmt, 1, chatIds[index]);
                bindInt64(update.stmt, 2, static_cast<std::int64_t>(index));
                stepDone(update.stmt);
            }
            commitTransaction();
        } catch (...) {
            rollbackTransaction();
            throw;
        }
    }

    PersistedChatState loadPersistedChatState() override {
        std::lock_guard<std::mutex> lock(mutex_);
        PersistedChatState state;

        Statement threadStmt(
            db_,
            "SELECT chat_id, title, updated_at_unix_ms, model_path, model_name "
            "FROM chat_threads "
            "ORDER BY sort_order ASC;"
        );

        while (sqlite3_step(threadStmt.stmt) == SQLITE_ROW) {
            ChatThread thread;
            thread.id = columnText(threadStmt.stmt, 0);
            thread.title = columnText(threadStmt.stmt, 1);
            thread.updatedAtUnixMs = sqlite3_column_int64(threadStmt.stmt, 2);
            thread.modelPath = columnText(threadStmt.stmt, 3);
            thread.modelName = columnText(threadStmt.stmt, 4);
            thread.streaming = false;

            Statement messageStmt(
                db_,
                "SELECT "
                "  m.role, m.text, m.started_at_nanos, m.finished_at_nanos, m.collapsed,"
                "  s.model_path, s.model_name, s.prompt_tokens, s.completion_tokens, s.started_at_unix_ms,"
                "  s.first_token_at_unix_ms, s.finished_at_unix_ms, s.tokens_per_second, s.status, s.error_text,"
                "  s.temp, s.top_p, s.top_k, s.max_tokens "
                "FROM chat_messages m "
                "LEFT JOIN chat_message_stats s "
                "  ON s.chat_id = m.chat_id AND s.message_order = m.message_order "
                "WHERE m.chat_id = ?1 "
                "ORDER BY m.message_order ASC;"
            );
            bindText(messageStmt.stmt, 1, thread.id);
            while (sqlite3_step(messageStmt.stmt) == SQLITE_ROW) {
                ChatMessage message;
                message.role = chatRoleFromStorage(columnText(messageStmt.stmt, 0));
                message.text = columnText(messageStmt.stmt, 1);
                message.startedAtNanos = sqlite3_column_int64(messageStmt.stmt, 2);
                message.finishedAtNanos = sqlite3_column_int64(messageStmt.stmt, 3);
                message.collapsed = sqlite3_column_int(messageStmt.stmt, 4) != 0;
                if (!columnIsNull(messageStmt.stmt, 5)) {
                    message.generationStats = MessageGenerationStats {
                        .modelPath = columnText(messageStmt.stmt, 5),
                        .modelName = columnText(messageStmt.stmt, 6),
                        .promptTokens = sqlite3_column_int64(messageStmt.stmt, 7),
                        .completionTokens = sqlite3_column_int64(messageStmt.stmt, 8),
                        .startedAtUnixMs = sqlite3_column_int64(messageStmt.stmt, 9),
                        .firstTokenAtUnixMs = sqlite3_column_int64(messageStmt.stmt, 10),
                        .finishedAtUnixMs = sqlite3_column_int64(messageStmt.stmt, 11),
                        .tokensPerSecond = columnDouble(messageStmt.stmt, 12),
                        .status = columnText(messageStmt.stmt, 13),
                        .errorText = columnText(messageStmt.stmt, 14),
                        .temp = static_cast<float>(columnDouble(messageStmt.stmt, 15)),
                        .topP = static_cast<float>(columnDouble(messageStmt.stmt, 16)),
                        .topK = static_cast<std::int32_t>(sqlite3_column_int64(messageStmt.stmt, 17)),
                        .maxTokens = static_cast<std::int32_t>(sqlite3_column_int64(messageStmt.stmt, 18)),
                    };
                }
                syncAssistantParagraphs(message);
                thread.messages.push_back(std::move(message));
            }

            state.chats.push_back(std::move(thread));
        }

        state.selectedChatId = loadPreference("selected_chat_id");
        return state;
    }

  private:
    static constexpr int kCurrentSchemaVersion = 6;

    struct Statement {
        sqlite3_stmt *stmt = nullptr;

        Statement(sqlite3 *db, char const *sql) {
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                throw std::runtime_error("Failed to prepare statement");
            }
        }

        ~Statement() {
            if (stmt != nullptr) {
                sqlite3_finalize(stmt);
                stmt = nullptr;
            }
        }
    };

    static std::filesystem::path defaultDataDirectory() {
        if (char const *overridePath = std::getenv("LAMBDA_STUDIO_DATA_DIR"); overridePath != nullptr && *overridePath) {
            return std::filesystem::path(overridePath);
        }
        if (char const *home = std::getenv("HOME"); home != nullptr && *home) {
#if defined(__APPLE__)
            return std::filesystem::path(home) / "Library" / "Application Support" / "Lambda Studio";
#else
            return std::filesystem::path(home) / ".lambda-studio";
#endif
        }
        return std::filesystem::temp_directory_path() / "lambda-studio";
    }

    static void bindText(sqlite3_stmt *stmt, int index, std::string const &value) {
        sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
    }

    static void bindInt64(sqlite3_stmt *stmt, int index, std::int64_t value) {
        sqlite3_bind_int64(stmt, index, value);
    }

    static void bindDouble(sqlite3_stmt *stmt, int index, double value) {
        sqlite3_bind_double(stmt, index, value);
    }

    static void bindBool(sqlite3_stmt *stmt, int index, bool value) {
        sqlite3_bind_int(stmt, index, value ? 1 : 0);
    }

    static std::int64_t currentUnixMillis() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    static void stepDone(sqlite3_stmt *stmt) {
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            throw std::runtime_error("SQLite statement failed");
        }
    }

    static std::string columnText(sqlite3_stmt *stmt, int index) {
        unsigned char const *value = sqlite3_column_text(stmt, index);
        return value == nullptr ? std::string {} : std::string(reinterpret_cast<char const *>(value));
    }

    static double columnDouble(sqlite3_stmt *stmt, int index) {
        return sqlite3_column_double(stmt, index);
    }

    static bool columnIsNull(sqlite3_stmt *stmt, int index) {
        return sqlite3_column_type(stmt, index) == SQLITE_NULL;
    }

    static RemoteModel readRemoteModelRow(sqlite3_stmt *stmt) {
        RemoteModel model;
        model.id = columnText(stmt, 0);
        model.author = columnText(stmt, 1);
        model.libraryName = columnText(stmt, 2);
        model.pipelineTag = columnText(stmt, 3);
        model.createdAt = columnText(stmt, 4);
        model.lastModified = columnText(stmt, 5);
        model.tags = splitTags(columnText(stmt, 6));
        model.downloads = sqlite3_column_int64(stmt, 7);
        model.downloadsAllTime = sqlite3_column_int64(stmt, 8);
        model.likes = sqlite3_column_int64(stmt, 9);
        model.usedStorage = sqlite3_column_int64(stmt, 10);
        model.gated = sqlite3_column_int(stmt, 11) != 0;
        model.isPrivate = sqlite3_column_int(stmt, 12) != 0;
        model.disabled = sqlite3_column_int(stmt, 13) != 0;
        return model;
    }

    static std::string joinTags(std::vector<std::string> const &tags) {
        std::string result;
        for (std::string const &tag : tags) {
            if (tag.find('\n') != std::string::npos) {
                continue;
            }
            if (!result.empty()) {
                result += '\n';
            }
            result += tag;
        }
        return result;
    }

    static std::vector<std::string> splitTags(std::string_view value) {
        std::vector<std::string> tags;
        std::size_t start = 0;
        while (start < value.size()) {
            std::size_t end = value.find('\n', start);
            if (end == std::string_view::npos) {
                end = value.size();
            }
            if (end > start) {
                tags.emplace_back(value.substr(start, end - start));
            }
            start = end + 1;
        }
        return tags;
    }

    static bool hasGgufExtension(std::string const &path) {
        return path.size() >= 5 && path.substr(path.size() - 5) == ".gguf";
    }

    static char const *downloadJobStatusStorage(DownloadJobStatus status) {
        switch (status) {
        case DownloadJobStatus::Running:
            return "running";
        case DownloadJobStatus::Completed:
            return "completed";
        case DownloadJobStatus::Failed:
            return "failed";
        }
        return "running";
    }

    static DownloadJobStatus downloadJobStatusFromStorage(std::string const &status) {
        if (status == "completed") {
            return DownloadJobStatus::Completed;
        }
        if (status == "failed") {
            return DownloadJobStatus::Failed;
        }
        return DownloadJobStatus::Running;
    }

    static char const *chatRoleStorage(ChatRole role) {
        switch (role) {
        case ChatRole::User:
            return "user";
        case ChatRole::Reasoning:
            return "reasoning";
        case ChatRole::Assistant:
            return "assistant";
        }
        return "assistant";
    }

    static ChatRole chatRoleFromStorage(std::string const &role) {
        if (role == "user") {
            return ChatRole::User;
        }
        if (role == "reasoning") {
            return ChatRole::Reasoning;
        }
        return ChatRole::Assistant;
    }

    std::vector<RemoteModel> loadSearchResultsFromPayload(std::string const &query) {
        Statement stmt(db_, "SELECT raw_json FROM search_payloads WHERE query = ?1;");
        bindText(stmt.stmt, 1, query);
        if (sqlite3_step(stmt.stmt) != SQLITE_ROW) {
            return {};
        }

        auto json = nlohmann::json::parse(columnText(stmt.stmt, 0), nullptr, false);
        if (!json.is_array()) {
            return {};
        }

        std::vector<RemoteModel> models;
        for (auto const &item : json) {
            if (!item.is_object()) {
                continue;
            }
            RemoteModel model;
            if (item.contains("id") && item["id"].is_string()) {
                model.id = item["id"].get<std::string>();
            }
            if (item.contains("author") && item["author"].is_string()) {
                model.author = item["author"].get<std::string>();
            }
            if (item.contains("library_name") && item["library_name"].is_string()) {
                model.libraryName = item["library_name"].get<std::string>();
            }
            if (item.contains("pipeline_tag") && item["pipeline_tag"].is_string()) {
                model.pipelineTag = item["pipeline_tag"].get<std::string>();
            }
            if (item.contains("createdAt") && item["createdAt"].is_string()) {
                model.createdAt = item["createdAt"].get<std::string>();
            }
            if (item.contains("lastModified") && item["lastModified"].is_string()) {
                model.lastModified = item["lastModified"].get<std::string>();
            }
            if (item.contains("downloads") && item["downloads"].is_number_integer()) {
                model.downloads = item["downloads"].get<std::int64_t>();
            }
            if (item.contains("downloadsAllTime") && item["downloadsAllTime"].is_number_integer()) {
                model.downloadsAllTime = item["downloadsAllTime"].get<std::int64_t>();
            }
            if (item.contains("likes") && item["likes"].is_number_integer()) {
                model.likes = item["likes"].get<std::int64_t>();
            }
            if (item.contains("usedStorage") && item["usedStorage"].is_number_integer()) {
                model.usedStorage = item["usedStorage"].get<std::int64_t>();
            }
            if (item.contains("gated") && item["gated"].is_boolean()) {
                model.gated = item["gated"].get<bool>();
            }
            if (item.contains("private") && item["private"].is_boolean()) {
                model.isPrivate = item["private"].get<bool>();
            }
            if (item.contains("disabled") && item["disabled"].is_boolean()) {
                model.disabled = item["disabled"].get<bool>();
            }
            if (item.contains("tags") && item["tags"].is_array()) {
                for (auto const &tag : item["tags"]) {
                    if (tag.is_string()) {
                        model.tags.push_back(tag.get<std::string>());
                    }
                }
            }
            if (!model.id.empty()) {
                models.push_back(std::move(model));
            }
        }
        return models;
    }

    std::vector<RemoteModelFile> loadRepoFilesFromPayload(std::string const &repoId) {
        Statement stmt(db_, "SELECT raw_json FROM repo_file_payloads WHERE repo_id = ?1;");
        bindText(stmt.stmt, 1, repoId);
        if (sqlite3_step(stmt.stmt) != SQLITE_ROW) {
            return {};
        }

        auto json = nlohmann::json::parse(columnText(stmt.stmt, 0), nullptr, false);
        if (!json.is_object() || !json.contains("tree") || !json["tree"].is_array()) {
            return {};
        }

        std::vector<RemoteModelFile> files;
        for (auto const &item : json["tree"]) {
            if (!item.is_object() || !item.contains("type") || !item["type"].is_string() ||
                item["type"].get<std::string>() != "file" || !item.contains("path") || !item["path"].is_string()) {
                continue;
            }

            std::string const path = item["path"].get<std::string>();
            if (!hasGgufExtension(path)) {
                continue;
            }

            RemoteModelFile file;
            file.repoId = repoId;
            file.path = path;
            if (item.contains("lfs") && item["lfs"].is_object() && item["lfs"].contains("size") &&
                item["lfs"]["size"].is_number_integer()) {
                file.sizeBytes = item["lfs"]["size"].get<std::size_t>();
            } else if (item.contains("size") && item["size"].is_number_integer()) {
                file.sizeBytes = item["size"].get<std::size_t>();
            }
            files.push_back(std::move(file));
        }
        return files;
    }

    static std::string arrayOrStringToJoinedText(nlohmann::json const &jsonValue) {
        if (jsonValue.is_string()) {
            return jsonValue.get<std::string>();
        }
        if (!jsonValue.is_array()) {
            return {};
        }

        std::string result;
        for (auto const &item : jsonValue) {
            if (!item.is_string()) {
                continue;
            }
            if (!result.empty()) {
                result += ", ";
            }
            result += item.get<std::string>();
        }
        return result;
    }

    static std::vector<std::string> arrayOrStringToVector(nlohmann::json const &jsonValue) {
        if (jsonValue.is_string()) {
            return {jsonValue.get<std::string>()};
        }
        if (!jsonValue.is_array()) {
            return {};
        }

        std::vector<std::string> values;
        for (auto const &item : jsonValue) {
            if (item.is_string()) {
                values.push_back(item.get<std::string>());
            }
        }
        return values;
    }

    static std::optional<RemoteRepoDetail> parseRepoDetailPayload(std::string const &rawJson) {
        auto json = nlohmann::json::parse(rawJson, nullptr, false);
        if (!json.is_object() || !json.contains("detail") || !json["detail"].is_object()) {
            return std::nullopt;
        }

        auto const &detailJson = json["detail"];
        RemoteRepoDetail detail;
        if (detailJson.contains("id") && detailJson["id"].is_string()) {
            detail.id = detailJson["id"].get<std::string>();
        }
        if (detailJson.contains("author") && detailJson["author"].is_string()) {
            detail.author = detailJson["author"].get<std::string>();
        }
        if (detailJson.contains("sha") && detailJson["sha"].is_string()) {
            detail.sha = detailJson["sha"].get<std::string>();
        }
        if (detailJson.contains("library_name") && detailJson["library_name"].is_string()) {
            detail.libraryName = detailJson["library_name"].get<std::string>();
        }
        if (detailJson.contains("pipeline_tag") && detailJson["pipeline_tag"].is_string()) {
            detail.pipelineTag = detailJson["pipeline_tag"].get<std::string>();
        }
        if (detailJson.contains("createdAt") && detailJson["createdAt"].is_string()) {
            detail.createdAt = detailJson["createdAt"].get<std::string>();
        }
        if (detailJson.contains("lastModified") && detailJson["lastModified"].is_string()) {
            detail.lastModified = detailJson["lastModified"].get<std::string>();
        }
        if (detailJson.contains("downloads") && detailJson["downloads"].is_number_integer()) {
            detail.downloads = detailJson["downloads"].get<std::int64_t>();
        }
        if (detailJson.contains("downloadsAllTime") && detailJson["downloadsAllTime"].is_number_integer()) {
            detail.downloadsAllTime = detailJson["downloadsAllTime"].get<std::int64_t>();
        }
        if (detailJson.contains("likes") && detailJson["likes"].is_number_integer()) {
            detail.likes = detailJson["likes"].get<std::int64_t>();
        }
        if (detailJson.contains("usedStorage") && detailJson["usedStorage"].is_number_integer()) {
            detail.usedStorage = detailJson["usedStorage"].get<std::int64_t>();
        }
        if (detailJson.contains("gated") && detailJson["gated"].is_boolean()) {
            detail.gated = detailJson["gated"].get<bool>();
        }
        if (detailJson.contains("private") && detailJson["private"].is_boolean()) {
            detail.isPrivate = detailJson["private"].get<bool>();
        }
        if (detailJson.contains("disabled") && detailJson["disabled"].is_boolean()) {
            detail.disabled = detailJson["disabled"].get<bool>();
        }
        if (detailJson.contains("tags") && detailJson["tags"].is_array()) {
            for (auto const &tag : detailJson["tags"]) {
                if (tag.is_string()) {
                    detail.tags.push_back(tag.get<std::string>());
                }
            }
        }

        if (detailJson.contains("cardData") && detailJson["cardData"].is_object()) {
            auto const &cardData = detailJson["cardData"];
            if (cardData.contains("license")) {
                detail.license = arrayOrStringToJoinedText(cardData["license"]);
            }
            if (cardData.contains("summary")) {
                detail.summary = arrayOrStringToJoinedText(cardData["summary"]);
            }
            if (detail.summary.empty() && cardData.contains("description")) {
                detail.summary = arrayOrStringToJoinedText(cardData["description"]);
            }
            if (cardData.contains("language")) {
                detail.languages = arrayOrStringToVector(cardData["language"]);
            }
            if (cardData.contains("base_model")) {
                detail.baseModels = arrayOrStringToVector(cardData["base_model"]);
            }
            if (detail.baseModels.empty() && cardData.contains("base_models")) {
                detail.baseModels = arrayOrStringToVector(cardData["base_models"]);
            }
        }

        if (json.contains("readme") && json["readme"].is_string()) {
            detail.readme = json["readme"].get<std::string>();
        }

        if (detail.id.empty()) {
            return std::nullopt;
        }
        return detail;
    }

    void execute(char const *sql) {
        char *error = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &error) != SQLITE_OK) {
            std::string message = error != nullptr ? error : "sqlite exec failed";
            sqlite3_free(error);
            throw std::runtime_error(message);
        }
    }

    void beginTransaction() { execute("BEGIN IMMEDIATE;"); }
    void commitTransaction() { execute("COMMIT;"); }
    void rollbackTransaction() {
        char *error = nullptr;
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, &error);
        sqlite3_free(error);
    }

    int userVersion() const {
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(db_, "PRAGMA user_version;", -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to read schema version");
        }
        int version = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            version = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return version;
    }

    void setUserVersion(int version) {
        execute(("PRAGMA user_version=" + std::to_string(version) + ";").c_str());
    }

    void migrateSchema() {
        int version = userVersion();
        if (version < 1) {
            applyMigration1();
            setUserVersion(1);
            version = 1;
        }
        if (version < 2) {
            applyMigration2();
            setUserVersion(2);
            version = 2;
        }
        if (version < 3) {
            applyMigration3();
            setUserVersion(3);
            version = 3;
        }
        if (version < 4) {
            applyMigration4();
            setUserVersion(4);
            version = 4;
        }
        if (version < 5) {
            applyMigration5();
            setUserVersion(5);
            version = 5;
        }
        if (version < 6) {
            applyMigration6();
            setUserVersion(6);
        }
    }

    void applyMigration1() {
        execute(
            "CREATE TABLE IF NOT EXISTS model_repos ("
            "  repo_id TEXT PRIMARY KEY,"
            "  author TEXT,"
            "  library_name TEXT,"
            "  pipeline_tag TEXT,"
            "  created_at TEXT,"
            "  last_modified TEXT,"
            "  tags TEXT,"
            "  downloads INTEGER NOT NULL DEFAULT 0,"
            "  downloads_all_time INTEGER NOT NULL DEFAULT 0,"
            "  likes INTEGER NOT NULL DEFAULT 0,"
            "  used_storage INTEGER NOT NULL DEFAULT 0,"
            "  gated INTEGER NOT NULL DEFAULT 0,"
            "  is_private INTEGER NOT NULL DEFAULT 0,"
            "  disabled INTEGER NOT NULL DEFAULT 0"
            ");"
            "CREATE TABLE IF NOT EXISTS search_results ("
            "  query TEXT NOT NULL,"
            "  repo_id TEXT NOT NULL,"
            "  rank_index INTEGER NOT NULL,"
            "  PRIMARY KEY (query, repo_id)"
            ");"
            "CREATE INDEX IF NOT EXISTS idx_search_results_query_rank ON search_results(query, rank_index);"
            "CREATE TABLE IF NOT EXISTS repo_files ("
            "  repo_id TEXT NOT NULL,"
            "  path TEXT NOT NULL,"
            "  local_path TEXT,"
            "  size_bytes INTEGER NOT NULL DEFAULT 0,"
            "  cached INTEGER NOT NULL DEFAULT 0,"
            "  PRIMARY KEY (repo_id, path)"
            ");"
            "CREATE TABLE IF NOT EXISTS search_payloads ("
            "  query TEXT PRIMARY KEY,"
            "  raw_json TEXT NOT NULL,"
            "  fetched_at_unix_ms INTEGER NOT NULL"
            ");"
            "CREATE TABLE IF NOT EXISTS repo_file_payloads ("
            "  repo_id TEXT PRIMARY KEY,"
            "  raw_json TEXT NOT NULL,"
            "  fetched_at_unix_ms INTEGER NOT NULL"
            ");"
        );
    }

    void applyMigration2() {
        execute(
            "CREATE TABLE IF NOT EXISTS repo_details ("
            "  repo_id TEXT PRIMARY KEY,"
            "  sha TEXT,"
            "  license TEXT,"
            "  summary TEXT,"
            "  readme_text TEXT,"
            "  raw_json TEXT NOT NULL,"
            "  fetched_at_unix_ms INTEGER NOT NULL"
            ");"
        );
    }

    void applyMigration3() {
        execute(
            "CREATE TABLE IF NOT EXISTS download_jobs ("
            "  job_id TEXT PRIMARY KEY,"
            "  repo_id TEXT NOT NULL,"
            "  file_path TEXT NOT NULL,"
            "  local_path TEXT NOT NULL DEFAULT '',"
            "  error_text TEXT NOT NULL DEFAULT '',"
            "  status TEXT NOT NULL,"
            "  started_at_unix_ms INTEGER NOT NULL,"
            "  finished_at_unix_ms INTEGER NOT NULL DEFAULT 0"
            ");"
            "CREATE INDEX IF NOT EXISTS idx_download_jobs_started ON download_jobs(started_at_unix_ms DESC);"
        );
    }

    void applyMigration4() {
        execute(
            "CREATE TABLE IF NOT EXISTS local_model_instances ("
            "  path TEXT PRIMARY KEY,"
            "  name TEXT NOT NULL,"
            "  repo_id TEXT NOT NULL DEFAULT '',"
            "  tag TEXT NOT NULL DEFAULT '',"
            "  size_bytes INTEGER NOT NULL DEFAULT 0,"
            "  discovered_at_unix_ms INTEGER NOT NULL"
            ");"
            "CREATE INDEX IF NOT EXISTS idx_local_model_instances_name ON local_model_instances(name ASC);"
        );
    }

    void applyMigration5() {
        execute(
            "CREATE TABLE IF NOT EXISTS app_preferences ("
            "  pref_key TEXT PRIMARY KEY,"
            "  value_text TEXT NOT NULL DEFAULT ''"
            ");"
            "CREATE TABLE IF NOT EXISTS chat_threads ("
            "  chat_id TEXT PRIMARY KEY,"
            "  title TEXT NOT NULL,"
            "  updated_at_unix_ms INTEGER NOT NULL DEFAULT 0,"
            "  model_path TEXT NOT NULL DEFAULT '',"
            "  model_name TEXT NOT NULL DEFAULT '',"
            "  sort_order INTEGER NOT NULL"
            ");"
            "CREATE INDEX IF NOT EXISTS idx_chat_threads_order ON chat_threads(sort_order ASC);"
            "CREATE TABLE IF NOT EXISTS chat_messages ("
            "  chat_id TEXT NOT NULL,"
            "  message_order INTEGER NOT NULL,"
            "  role TEXT NOT NULL,"
            "  text TEXT NOT NULL DEFAULT '',"
            "  started_at_nanos INTEGER NOT NULL DEFAULT 0,"
            "  finished_at_nanos INTEGER NOT NULL DEFAULT 0,"
            "  collapsed INTEGER NOT NULL DEFAULT 0,"
            "  PRIMARY KEY (chat_id, message_order)"
            ");"
            "CREATE INDEX IF NOT EXISTS idx_chat_messages_chat_order "
            "ON chat_messages(chat_id, message_order ASC);"
        );
    }

    void applyMigration6() {
        execute(
            "CREATE TABLE IF NOT EXISTS chat_message_stats ("
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
            "CREATE INDEX IF NOT EXISTS idx_chat_message_stats_chat_order "
            "ON chat_message_stats(chat_id, message_order ASC);"
        );
    }

    std::string loadPreference(std::string const &key) {
        Statement stmt(
            db_,
            "SELECT value_text FROM app_preferences WHERE pref_key = ?1;"
        );
        bindText(stmt.stmt, 1, key);
        if (sqlite3_step(stmt.stmt) != SQLITE_ROW) {
            return {};
        }
        return columnText(stmt.stmt, 0);
    }

    void tryOpen(std::filesystem::path path) {
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        if (ec) {
            path = std::filesystem::temp_directory_path() / "lambda-studio";
            std::filesystem::create_directories(path);
        }

        dataDirectory_ = std::move(path);
        if (sqlite3_open_v2(databasePath().c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
            std::string message = db_ != nullptr ? sqlite3_errmsg(db_) : "sqlite open failed";
            if (db_ != nullptr) {
                sqlite3_close(db_);
                db_ = nullptr;
            }
            throw std::runtime_error("Failed to open model catalog: " + message);
        }

        execute("PRAGMA journal_mode=WAL;");
        migrateSchema();
    }

    std::filesystem::path dataDirectory_;
    sqlite3 *db_ = nullptr;
    std::mutex mutex_;
};

} // namespace lambda
