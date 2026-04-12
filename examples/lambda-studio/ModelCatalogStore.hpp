#pragma once

#include "AppState.hpp"

#include <sqlite3.h>

#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lambda {

class ModelCatalogStore {
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

    std::filesystem::path databasePath() const {
        return dataDirectory_ / "model_catalog.sqlite3";
    }

    void replaceSearchResults(std::string const &query, std::vector<RemoteModel> const &models) {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            beginTransaction();

            Statement deleteSearch(db_, "DELETE FROM search_results WHERE query = ?1;");
            bindText(deleteSearch.stmt, 1, query);
            stepDone(deleteSearch.stmt);

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

    std::vector<RemoteModel> loadSearchResults(std::string const &query) {
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
            RemoteModel model;
            model.id = columnText(stmt.stmt, 0);
            model.author = columnText(stmt.stmt, 1);
            model.libraryName = columnText(stmt.stmt, 2);
            model.pipelineTag = columnText(stmt.stmt, 3);
            model.createdAt = columnText(stmt.stmt, 4);
            model.lastModified = columnText(stmt.stmt, 5);
            model.tags = splitTags(columnText(stmt.stmt, 6));
            model.downloads = sqlite3_column_int64(stmt.stmt, 7);
            model.downloadsAllTime = sqlite3_column_int64(stmt.stmt, 8);
            model.likes = sqlite3_column_int64(stmt.stmt, 9);
            model.usedStorage = sqlite3_column_int64(stmt.stmt, 10);
            model.gated = sqlite3_column_int(stmt.stmt, 11) != 0;
            model.isPrivate = sqlite3_column_int(stmt.stmt, 12) != 0;
            model.disabled = sqlite3_column_int(stmt.stmt, 13) != 0;
            models.push_back(std::move(model));
        }
        return models;
    }

    void replaceRepoFiles(std::string const &repoId, std::vector<RemoteModelFile> const &files) {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            beginTransaction();

            Statement deleteFiles(db_, "DELETE FROM repo_files WHERE repo_id = ?1;");
            bindText(deleteFiles.stmt, 1, repoId);
            stepDone(deleteFiles.stmt);

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

    std::vector<RemoteModelFile> loadRepoFiles(std::string const &repoId) {
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
        return files;
    }

  private:
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

    static void bindBool(sqlite3_stmt *stmt, int index, bool value) {
        sqlite3_bind_int(stmt, index, value ? 1 : 0);
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

        execute(
            "PRAGMA journal_mode=WAL;"
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
        );
    }

    std::filesystem::path dataDirectory_;
    sqlite3 *db_ = nullptr;
    std::mutex mutex_;
};

} // namespace lambda
