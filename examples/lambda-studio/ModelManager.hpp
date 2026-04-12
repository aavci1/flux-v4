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
#include <chrono>
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

struct HfRepoFilesResponse {
    std::vector<HfFileInfo> files;
    std::string rawJson;
};

struct HfRepoDetailResponse {
    HfRepoDetailInfo detail;
    std::string rawJson;
};

class ModelManager {
  public:
    using PostFn = std::function<void(ModelManagerEvent)>;

    explicit ModelManager(std::shared_ptr<LlamaEngine> engine, PostFn post)
        : engine_(std::move(engine)), post_(std::move(post)) {}

    ~ModelManager() { joinWorker(); }

    ModelManager(ModelManager const &) = delete;
    ModelManager &operator=(ModelManager const &) = delete;

    void refreshLocalModels() { runAsync([this] { postLocalModelsReady_(); }); }

    void searchHuggingFace(HfSearchRequest request) {
        runAsync([this, req = std::move(request)] {
            try {
                auto [results, rawJson] = searchHfSync(req);
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfSearchReady,
                    .hfModels = std::move(results),
                    .searchKey = req.cacheKey,
                    .rawJson = std::move(rawJson),
                });
            } catch (std::exception const &e) {
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfSearchReady,
                    .error = e.what(),
                    .searchKey = req.cacheKey,
                });
            }
        });
    }

    void listRepoFiles(std::string repoId) {
        runAsync([this, repo = std::move(repoId)] {
            try {
                auto response = listRepoFilesSync(repo);
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfFilesReady,
                    .hfFiles = std::move(response.files),
                    .repoId = repo,
                    .rawJson = std::move(response.rawJson),
                });
            } catch (std::exception const &e) {
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfFilesReady,
                    .error = e.what(),
                    .repoId = repo,
                });
            }
        });
    }

    void fetchRepoDetail(std::string repoId) {
        runAsync([this, repo = std::move(repoId)] {
            try {
                auto response = fetchRepoDetailSync(repo);
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfRepoDetailReady,
                    .hfRepoDetail = std::move(response.detail),
                    .repoId = repo,
                    .rawJson = std::move(response.rawJson),
                });
            } catch (std::exception const &e) {
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfRepoDetailReady,
                    .error = e.what(),
                    .repoId = repo,
                });
            }
        });
    }

    void inspectRepo(std::string repoId) {
        runAsync([this, repo = std::move(repoId)] {
            try {
                auto detailResponse = fetchRepoDetailSync(repo);
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfRepoDetailReady,
                    .hfRepoDetail = std::move(detailResponse.detail),
                    .repoId = repo,
                    .rawJson = std::move(detailResponse.rawJson),
                });
            } catch (std::exception const &e) {
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfRepoDetailReady,
                    .error = e.what(),
                    .repoId = repo,
                });
            }

            try {
                auto filesResponse = listRepoFilesSync(repo);
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfFilesReady,
                    .hfFiles = std::move(filesResponse.files),
                    .repoId = repo,
                    .rawJson = std::move(filesResponse.rawJson),
                });
            } catch (std::exception const &e) {
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfFilesReady,
                    .error = e.what(),
                    .repoId = repo,
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
                struct DownloadProgressContext {
                    ModelManager * manager = nullptr;
                    std::string repoId;
                    std::string filePath;
                    std::mutex mutex;
                    std::size_t lastDownloadedBytes = 0;
                    std::size_t lastTotalBytes = 0;
                    int lastPercent = -1;
                    std::chrono::steady_clock::time_point lastPostedAt {};
                };

                DownloadProgressContext progressContext {
                    .manager = this,
                    .repoId = repo,
                    .filePath = file,
                };

                common_download_model_opts opts;
                opts.progress_callback = +[](std::size_t downloadedBytes,
                                             std::size_t totalBytes,
                                             const char * /*itemPath*/,
                                             void * userData) {
                    auto * ctx = static_cast<DownloadProgressContext *>(userData);
                    if (ctx == nullptr || ctx->manager == nullptr || totalBytes == 0) {
                        return;
                    }

                    using clock = std::chrono::steady_clock;
                    auto const now = clock::now();
                    int const percent = totalBytes == 0 ? 0 :
                        static_cast<int>((100.0 * static_cast<double>(downloadedBytes)) / static_cast<double>(totalBytes));

                    bool shouldPost = false;
                    {
                        std::lock_guard<std::mutex> lock(ctx->mutex);
                        shouldPost =
                            downloadedBytes >= totalBytes ||
                            ctx->lastPercent < 0 ||
                            percent >= ctx->lastPercent + 1 ||
                            now - ctx->lastPostedAt >= std::chrono::milliseconds(120);
                        if (!shouldPost && downloadedBytes <= ctx->lastDownloadedBytes) {
                            return;
                        }
                        ctx->lastDownloadedBytes = downloadedBytes;
                        ctx->lastTotalBytes = totalBytes;
                        if (!shouldPost) {
                            return;
                        }
                        ctx->lastPercent = percent;
                        ctx->lastPostedAt = now;
                    }

                    ctx->manager->post_(ModelManagerEvent {
                        .kind = ModelManagerEvent::Kind::DownloadProgress,
                        .repoId = ctx->repoId,
                        .filePath = ctx->filePath,
                        .downloadedBytes = downloadedBytes,
                        .totalBytes = totalBytes,
                    });
                };
                opts.progress_callback_user_data = &progressContext;

                auto result = common_download_model(model, token, opts);
                if (result.model_path.empty()) {
                    post_(ModelManagerEvent {
                        .kind = ModelManagerEvent::Kind::DownloadError,
                        .error = "Download returned empty path",
                        .repoId = repo,
                        .filePath = file,
                    });
                    return;
                }

                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::DownloadDone,
                    .repoId = repo,
                    .filePath = file,
                    .modelPath = result.model_path,
                    .modelName = repo + "/" + file,
                    .downloadedBytes = progressContext.lastDownloadedBytes,
                    .totalBytes = progressContext.lastTotalBytes,
                });

                postLocalModelsReady_();
            } catch (std::exception const &e) {
                post_(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::DownloadError,
                    .error = e.what(),
                    .repoId = repo,
                    .filePath = file,
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

    std::pair<std::vector<HfModelInfo>, std::string> searchHfSync(HfSearchRequest const &request) const {
        std::string url = get_model_endpoint();
        url += "api/models?library=gguf&direction=-1&limit=20&full=true&cardData=true";
        if (!request.sortKey.empty()) {
            url += "&sort=" + urlEncode(request.sortKey);
        }
        if (!request.query.empty()) {
            url += "&search=" + urlEncode(request.query);
        }
        if (!request.author.empty()) {
            url += "&author=" + urlEncode(request.author);
        }

        common_remote_params params;
        params.timeout = 15;
        std::string const token = hfToken();
        if (!token.empty()) {
            params.headers.emplace_back("Authorization", "Bearer " + token);
        }
        auto [status, body] = common_remote_get_content(url, params);
        if (status != 200) {
            throw std::runtime_error("HF API returned " + std::to_string(status));
        }

        auto json = nlohmann::json::parse(body.begin(), body.end());
        std::vector<HfModelInfo> results;
        if (!json.is_array()) {
            return {results, std::string(body.begin(), body.end())};
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
                bool include = true;
                if (request.visibilityFilter == "public") {
                    include = !info.gated && !info.isPrivate && !info.disabled;
                } else if (request.visibilityFilter == "gated") {
                    include = info.gated;
                }
                if (include) {
                    results.push_back(std::move(info));
                }
            }
        }
        return {std::move(results), std::string(body.begin(), body.end())};
    }

    HfRepoFilesResponse listRepoFilesSync(std::string const &repoId) const {
        std::string const token = hfToken();
        std::string const commit = resolveRepoCommitSync(repoId, token);
        if (commit.empty()) {
            throw std::runtime_error("Failed to resolve repository revision");
        }

        std::string const body = fetchRepoTreeSync(repoId, commit, token);
        auto json = nlohmann::json::parse(body);
        auto const cachedFiles = hf_cache::get_cached_files(repoId);

        std::vector<HfFileInfo> ggufFiles;
        if (json.is_array()) {
            for (auto const &file : json) {
                if (!file.is_object() || !file.contains("type") || !file["type"].is_string() ||
                    file["type"].get<std::string>() != "file" || !file.contains("path") || !file["path"].is_string()) {
                    continue;
                }

                std::string const path = file["path"].get<std::string>();
                if (!hasGgufExtension(path)) {
                    continue;
                }

                std::size_t sizeBytes = 0;
                if (file.contains("lfs") && file["lfs"].is_object() && file["lfs"].contains("size") &&
                    file["lfs"]["size"].is_number()) {
                    sizeBytes = file["lfs"]["size"].get<std::size_t>();
                } else if (file.contains("size") && file["size"].is_number()) {
                    sizeBytes = file["size"].get<std::size_t>();
                }

                std::string localPath;
                bool cached = false;
                auto cachedIt = std::find_if(cachedFiles.begin(), cachedFiles.end(), [&](hf_cache::hf_file const &entry) {
                    return entry.path == path;
                });
                if (cachedIt != cachedFiles.end()) {
                    localPath = cachedIt->final_path;
                    cached = !localPath.empty() && fs::exists(localPath);
                }

                ggufFiles.push_back(HfFileInfo {
                    .repoId = repoId,
                    .path = path,
                    .sizeBytes = sizeBytes,
                    .localPath = std::move(localPath),
                    .cached = cached,
                });
            }
        }

        for (auto const &cached : cachedFiles) {
            if (!hasGgufExtension(cached.path)) {
                continue;
            }
            auto it = std::find_if(ggufFiles.begin(), ggufFiles.end(), [&](HfFileInfo const &file) {
                return file.path == cached.path;
            });
            if (it == ggufFiles.end()) {
                ggufFiles.push_back(HfFileInfo {
                    .repoId = repoId,
                    .path = cached.path,
                    .sizeBytes = cached.size,
                    .localPath = cached.final_path,
                    .cached = !cached.final_path.empty() && fs::exists(cached.final_path),
                });
            } else {
                it->localPath = cached.final_path;
                it->cached = !cached.final_path.empty() && fs::exists(cached.final_path);
            }
        }

        nlohmann::json payload = {
            {"repoId", repoId},
            {"commit", commit},
            {"tree", json},
        };

        return {
            .files = std::move(ggufFiles),
            .rawJson = payload.dump(),
        };
    }

    HfRepoDetailResponse fetchRepoDetailSync(std::string const &repoId) const {
        std::string const token = hfToken();
        std::string url = get_model_endpoint() + "api/models/" + repoId;
        common_remote_params params;
        params.timeout = 15;
        if (!token.empty()) {
            params.headers.emplace_back("Authorization", "Bearer " + token);
        }

        auto [status, body] = common_remote_get_content(url, params);
        if (status != 200) {
            throw std::runtime_error("HF repo detail API returned " + std::to_string(status));
        }

        auto json = nlohmann::json::parse(body.begin(), body.end());
        if (!json.is_object()) {
            throw std::runtime_error("HF repo detail payload was not an object");
        }

        HfRepoDetailInfo detail;
        if (json.contains("id") && json["id"].is_string()) {
            detail.id = json["id"].get<std::string>();
        }
        if (json.contains("author") && json["author"].is_string()) {
            detail.author = json["author"].get<std::string>();
        }
        if (json.contains("sha") && json["sha"].is_string()) {
            detail.sha = json["sha"].get<std::string>();
        }
        if (json.contains("library_name") && json["library_name"].is_string()) {
            detail.libraryName = json["library_name"].get<std::string>();
        }
        if (json.contains("pipeline_tag") && json["pipeline_tag"].is_string()) {
            detail.pipelineTag = json["pipeline_tag"].get<std::string>();
        }
        if (json.contains("createdAt") && json["createdAt"].is_string()) {
            detail.createdAt = json["createdAt"].get<std::string>();
        }
        if (json.contains("lastModified") && json["lastModified"].is_string()) {
            detail.lastModified = json["lastModified"].get<std::string>();
        }
        if (json.contains("downloads") && json["downloads"].is_number_integer()) {
            detail.downloads = json["downloads"].get<std::int64_t>();
        }
        if (json.contains("downloadsAllTime") && json["downloadsAllTime"].is_number_integer()) {
            detail.downloadsAllTime = json["downloadsAllTime"].get<std::int64_t>();
        }
        if (json.contains("likes") && json["likes"].is_number_integer()) {
            detail.likes = json["likes"].get<std::int64_t>();
        }
        if (json.contains("usedStorage") && json["usedStorage"].is_number_integer()) {
            detail.usedStorage = json["usedStorage"].get<std::int64_t>();
        }
        if (json.contains("gated") && json["gated"].is_boolean()) {
            detail.gated = json["gated"].get<bool>();
        }
        if (json.contains("private") && json["private"].is_boolean()) {
            detail.isPrivate = json["private"].get<bool>();
        }
        if (json.contains("disabled") && json["disabled"].is_boolean()) {
            detail.disabled = json["disabled"].get<bool>();
        }
        if (json.contains("tags") && json["tags"].is_array()) {
            for (auto const &tag : json["tags"]) {
                if (tag.is_string()) {
                    detail.tags.push_back(tag.get<std::string>());
                }
            }
        }
        if (json.contains("cardData") && json["cardData"].is_object()) {
            auto const &cardData = json["cardData"];
            if (cardData.contains("license")) {
                detail.license = joinJsonStrings(cardData["license"]);
            }
            if (cardData.contains("summary")) {
                detail.summary = joinJsonStrings(cardData["summary"]);
            }
            if (detail.summary.empty() && cardData.contains("description")) {
                detail.summary = joinJsonStrings(cardData["description"]);
            }
            if (cardData.contains("language")) {
                detail.languages = collectJsonStrings(cardData["language"]);
            }
            if (cardData.contains("base_model")) {
                detail.baseModels = collectJsonStrings(cardData["base_model"]);
            }
            if (detail.baseModels.empty() && cardData.contains("base_models")) {
                detail.baseModels = collectJsonStrings(cardData["base_models"]);
            }
        }

        std::string readme;
        std::string const readmeUrl = get_model_endpoint() + repoId + "/resolve/main/README.md";
        auto [readmeStatus, readmeBody] = common_remote_get_content(readmeUrl, params);
        if (readmeStatus == 200) {
            readme = std::string(readmeBody.begin(), readmeBody.end());
            detail.readme = readme;
        }

        nlohmann::json payload = {
            {"detail", json},
            {"readme", readme},
        };

        return {
            .detail = std::move(detail),
            .rawJson = payload.dump(),
        };
    }

    static std::string resolveRepoCommitSync(std::string const &repoId, std::string const &token) {
        std::string url = get_model_endpoint() + "api/models/" + repoId + "/refs";
        common_remote_params params;
        params.timeout = 15;
        if (!token.empty()) {
            params.headers.emplace_back("Authorization", "Bearer " + token);
        }

        auto [status, body] = common_remote_get_content(url, params);
        if (status != 200) {
            throw std::runtime_error("HF refs API returned " + std::to_string(status));
        }

        auto json = nlohmann::json::parse(body.begin(), body.end());
        if (!json.contains("branches") || !json["branches"].is_array()) {
            return {};
        }

        std::string fallback;
        for (auto const &branch : json["branches"]) {
            if (!branch.is_object() || !branch.contains("name") || !branch["name"].is_string() ||
                !branch.contains("targetCommit") || !branch["targetCommit"].is_string()) {
                continue;
            }
            std::string const name = branch["name"].get<std::string>();
            std::string const commit = branch["targetCommit"].get<std::string>();
            if (name == "main") {
                return commit;
            }
            if (fallback.empty()) {
                fallback = commit;
            }
        }
        return fallback;
    }

    static std::string fetchRepoTreeSync(
        std::string const &repoId,
        std::string const &commit,
        std::string const &token
    ) {
        std::string url = get_model_endpoint() + "api/models/" + repoId + "/tree/" + commit + "?recursive=true";
        common_remote_params params;
        params.timeout = 15;
        if (!token.empty()) {
            params.headers.emplace_back("Authorization", "Bearer " + token);
        }

        auto [status, body] = common_remote_get_content(url, params);
        if (status != 200) {
            throw std::runtime_error("HF tree API returned " + std::to_string(status));
        }
        return std::string(body.begin(), body.end());
    }

    static std::vector<std::string> collectJsonStrings(nlohmann::json const &jsonValue) {
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

    static std::string joinJsonStrings(nlohmann::json const &jsonValue) {
        auto values = collectJsonStrings(jsonValue);
        std::string result;
        for (std::string const &value : values) {
            if (value.empty()) {
                continue;
            }
            if (!result.empty()) {
                result += ", ";
            }
            result += value;
        }
        return result;
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
