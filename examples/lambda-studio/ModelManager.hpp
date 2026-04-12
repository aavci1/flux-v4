#pragma once

#include "BackendTypes.hpp"
#include "LlamaEngine.hpp"

#include "common.h"
#include "download.h"
#include "hf-cache.h"

#define JSON_ASSERT GGML_ASSERT
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace lambda_backend {

namespace fs = std::filesystem;

class ModelManager {
  public:
    using PostFn = std::function<void(ModelManagerEvent)>;

    explicit ModelManager(std::shared_ptr<LlamaEngine> engine, PostFn post)
        : engine_(std::move(engine)), post_(std::move(post)) {}

    ~ModelManager() { joinWorker(); }

    ModelManager(ModelManager const &) = delete;
    ModelManager &operator=(ModelManager const &) = delete;

    void refreshLocalModels() { runAsync([this] { postLocalModelsReady_(); }); }

    void searchHuggingFace(std::string query) {
        runAsync([this, q = std::move(query)] {
            try {
                auto results = searchHfSync(q);
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfSearchReady,
                    .hfModels = std::move(results),
                });
            } catch (std::exception const &e) {
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfSearchReady,
                    .error = e.what(),
                });
            }
        });
    }

    void listRepoFiles(std::string repoId) {
        runAsync([this, repo = std::move(repoId)] {
            try {
                auto files = listRepoFilesSync(repo);
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfFilesReady,
                    .hfFiles = std::move(files),
                });
            } catch (std::exception const &e) {
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfFilesReady,
                    .error = e.what(),
                });
            }
        });
    }

    void downloadModel(std::string repoId, std::string fileName) {
        runAsync([this, repo = std::move(repoId), file = std::move(fileName)] {
            try {
                common_params_model model;
                model.hf_repo = repo;
                model.hf_file = file;

                std::string const token = hfToken();
                auto result = common_download_model(model, token);
                if (result.model_path.empty()) {
                    post_(ModelManagerEvent {
                        .kind = ModelManagerEvent::Kind::DownloadError,
                        .error = "Download returned empty path",
                    });
                    return;
                }

                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::DownloadDone,
                    .modelPath = result.model_path,
                    .modelName = repo + "/" + file,
                });

                postLocalModelsReady_();
            } catch (std::exception const &e) {
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::DownloadError,
                    .error = e.what(),
                });
            }
        });
    }

    void loadModel(std::string path, int nGpuLayers = -1) {
        runAsync([this, modelPath = std::move(path), gpu = nGpuLayers] {
            bool const ok = engine_->load(modelPath, gpu);
            if (ok) {
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::ModelLoaded,
                    .modelPath = modelPath,
                    .modelName = extractModelName(modelPath),
                });
            } else {
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::ModelLoadError,
                    .error = std::string("Failed to load: ") + modelPath,
                });
            }
        });
    }

    void unloadModel() {
        engine_->cancelGeneration();
        engine_->unload();
    }

  private:
    void postLocalModelsReady_() {
        auto models = listLocalModelsSync();
        post_(ModelManagerEvent {
            .kind = ModelManagerEvent::Kind::LocalModelsReady,
            .localModels = std::move(models),
        });
    }

    void joinWorker() {
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    void runAsync(std::function<void()> fn) {
        joinWorker();
        worker_ = std::thread(std::move(fn));
    }

    static std::string hfToken() {
        if (auto *token = std::getenv("HF_TOKEN")) {
            return token;
        }
        if (auto *token = std::getenv("HUGGING_FACE_HUB_TOKEN")) {
            return token;
        }

        std::string tokenPath;
        if (auto *hfHome = std::getenv("HF_HOME")) {
            tokenPath = std::string(hfHome) + "/token";
        } else if (auto *home = std::getenv("HOME")) {
            tokenPath = std::string(home) + "/.cache/huggingface/token";
        }
        if (!tokenPath.empty() && fs::exists(tokenPath)) {
            std::ifstream file(tokenPath);
            std::string token;
            if (std::getline(file, token) && !token.empty()) {
                return token;
            }
        }
        return {};
    }

    std::vector<LocalModelInfo> listLocalModelsSync() const {
        std::vector<LocalModelInfo> models;
        std::unordered_set<std::string> seenPaths;

        appendCachedModels(models, seenPaths);
        appendExplicitModel(defaultModelPath(), models, seenPaths);
        scanDirectory(modelsDir(), models, seenPaths);

        std::sort(
            models.begin(),
            models.end(),
            [](LocalModelInfo const &lhs, LocalModelInfo const &rhs) {
                if (lhs.repo != rhs.repo) {
                    return lhs.repo < rhs.repo;
                }
                if (lhs.displayName() != rhs.displayName()) {
                    return lhs.displayName() < rhs.displayName();
                }
                return lhs.path < rhs.path;
            }
        );
        return models;
    }

    std::vector<HfModelInfo> searchHfSync(std::string const &query) const {
        std::string url = get_model_endpoint();
        url += "api/models?library=gguf&sort=downloads&direction=-1&limit=20";
        if (!query.empty()) {
            url += "&search=" + urlEncode(query);
        }

        common_remote_params params;
        params.timeout = 15;
        auto [status, body] = common_remote_get_content(url, params);
        if (status != 200) {
            throw std::runtime_error("HF API returned " + std::to_string(status));
        }

        auto json = nlohmann::json::parse(body.begin(), body.end());
        std::vector<HfModelInfo> results;
        if (!json.is_array()) {
            return results;
        }

        for (auto const &item : json) {
            HfModelInfo info;
            if (item.contains("id") && item["id"].is_string()) {
                info.id = item["id"].get<std::string>();
            }
            if (item.contains("author") && item["author"].is_string()) {
                info.author = item["author"].get<std::string>();
            }
            if (item.contains("library_name") && item["library_name"].is_string()) {
                info.libraryName = item["library_name"].get<std::string>();
            }
            if (item.contains("lastModified") && item["lastModified"].is_string()) {
                info.lastModified = item["lastModified"].get<std::string>();
            }
            if (item.contains("createdAt") && item["createdAt"].is_string()) {
                info.createdAt = item["createdAt"].get<std::string>();
            }
            if (item.contains("downloads") && item["downloads"].is_number()) {
                info.downloads = item["downloads"].get<std::int64_t>();
            }
            if (item.contains("downloadsAllTime") && item["downloadsAllTime"].is_number()) {
                info.downloadsAllTime = item["downloadsAllTime"].get<std::int64_t>();
            }
            if (item.contains("likes") && item["likes"].is_number()) {
                info.likes = item["likes"].get<std::int64_t>();
            }
            if (item.contains("usedStorage") && item["usedStorage"].is_number()) {
                info.usedStorage = item["usedStorage"].get<std::int64_t>();
            }
            if (item.contains("pipeline_tag") && item["pipeline_tag"].is_string()) {
                info.pipelineTag = item["pipeline_tag"].get<std::string>();
            }
            if (item.contains("gated") && item["gated"].is_boolean()) {
                info.gated = item["gated"].get<bool>();
            }
            if (item.contains("private") && item["private"].is_boolean()) {
                info.isPrivate = item["private"].get<bool>();
            }
            if (item.contains("disabled") && item["disabled"].is_boolean()) {
                info.disabled = item["disabled"].get<bool>();
            }
            if (item.contains("tags") && item["tags"].is_array()) {
                for (auto const &tag : item["tags"]) {
                    if (tag.is_string()) {
                        info.tags.push_back(tag.get<std::string>());
                    }
                }
            }
            if (!info.id.empty()) {
                results.push_back(std::move(info));
            }
        }
        return results;
    }

    std::vector<HfFileInfo> listRepoFilesSync(std::string const &repoId) const {
        std::string const token = hfToken();
        auto const files = hf_cache::get_repo_files(repoId, token);

        std::vector<HfFileInfo> ggufFiles;
        for (auto const &file : files) {
            if (file.path.size() >= 5 && file.path.substr(file.path.size() - 5) == ".gguf") {
                ggufFiles.push_back(HfFileInfo {
                    .repoId = repoId,
                    .path = file.path,
                    .sizeBytes = file.size,
                    .localPath = file.final_path,
                    .cached = !file.final_path.empty() && fs::exists(file.final_path),
                });
            }
        }
        return ggufFiles;
    }

    static void appendCachedModels(std::vector<LocalModelInfo> &out, std::unordered_set<std::string> &seenPaths) {
        for (auto const &file : hf_cache::get_cached_files()) {
            if (!hasGgufExtension(file.path)) {
                continue;
            }
            appendModel(out, seenPaths, file.local_path, file.repo_id, {});
        }
    }

    static void appendExplicitModel(
        std::string const &path,
        std::vector<LocalModelInfo> &out,
        std::unordered_set<std::string> &seenPaths
    ) {
        if (!hasGgufExtension(path)) {
            return;
        }
        appendModel(out, seenPaths, path, {}, {});
    }

    static void scanDirectory(
        std::string const &dir,
        std::vector<LocalModelInfo> &out,
        std::unordered_set<std::string> &seenPaths
    ) {
        if (dir.empty() || !fs::is_directory(dir)) {
            return;
        }
        try {
            for (auto const &entry : fs::recursive_directory_iterator(dir)) {
                if (!entry.is_regular_file()) {
                    continue;
                }
                auto const path = entry.path().lexically_normal().string();
                if (hasGgufExtension(path)) {
                    appendModel(out, seenPaths, path, {}, {});
                }
            }
        } catch (...) {
        }
    }

    static bool hasGgufExtension(std::string const &path) {
        return path.size() >= 5 && path.substr(path.size() - 5) == ".gguf";
    }

    static std::string modelsDir() {
        if (auto *home = std::getenv("HOME")) {
            return std::string(home) + "/.lambda-studio/models";
        }
        return {};
    }

    static std::string extractModelName(std::string const &path) {
        auto const slash = path.rfind('/');
        std::string name = slash != std::string::npos ? path.substr(slash + 1) : path;
        if (name.size() > 5 && name.substr(name.size() - 5) == ".gguf") {
            name.resize(name.size() - 5);
        }
        return name;
    }

    static std::string urlEncode(std::string const &text) {
        std::string encoded;
        for (unsigned char c : text) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded += static_cast<char>(c);
            } else {
                char buf[4];
                std::snprintf(buf, sizeof(buf), "%%%02X", c);
                encoded += buf;
            }
        }
        return encoded;
    }

    static void appendModel(
        std::vector<LocalModelInfo> &out,
        std::unordered_set<std::string> &seenPaths,
        std::string const &path,
        std::string repo,
        std::string tag
    ) {
        if (path.empty()) {
            return;
        }

        std::error_code ec;
        fs::path const normalized = fs::path(path).lexically_normal();
        if (!fs::is_regular_file(normalized, ec)) {
            return;
        }
        ec.clear();
        fs::path const resolved = fs::weakly_canonical(normalized, ec);
        std::string const key = ec ? normalized.string() : resolved.string();

        if (!seenPaths.insert(key).second) {
            return;
        }

        std::uintmax_t const size = fs::file_size(normalized, ec);
        out.push_back(LocalModelInfo {
            .repo = std::move(repo),
            .tag = std::move(tag),
            .path = normalized.string(),
            .sizeBytes = ec ? 0 : static_cast<std::size_t>(size),
        });
    }

    std::shared_ptr<LlamaEngine> engine_;
    PostFn post_;
    std::thread worker_;
};

} // namespace lambda_backend
