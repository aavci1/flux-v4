#pragma once

#include "Interfaces.hpp"
#include "Types.hpp"
#include "Defaults.hpp"

#include "common.h"
#include "download.h"
#include "hf-cache.h"

#define JSON_ASSERT GGML_ASSERT
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace lambda_studio_backend {

namespace fs = std::filesystem;

struct HfRepoFilesResponse {
    std::vector<HfFileInfo> files;
    std::string rawJson;
};

struct HfRepoDetailResponse {
    HfRepoDetailInfo detail;
    std::string rawJson;
};

class ModelManager : public lambda::IModelManager {
  public:
    using PostFn = std::function<void(ModelManagerEvent)>;

    explicit ModelManager(std::shared_ptr<lambda::IChatEngine> engine, PostFn post)
        : engine_(std::move(engine)),
          post_(std::move(post)),
          inventoryLane_(true),
          searchLane_(true),
          repoInspectLane_(true),
          downloadLane_(false),
          loadModelLane_(false) {
        startLane(inventoryLane_);
        startLane(searchLane_);
        startLane(repoInspectLane_);
        startLane(downloadLane_);
        startLane(loadModelLane_);
    }

    ~ModelManager() override {
        shuttingDown_.store(true, std::memory_order_relaxed);
        downloadCancelRequested_.store(true, std::memory_order_relaxed);
        stopLane(inventoryLane_);
        stopLane(searchLane_);
        stopLane(repoInspectLane_);
        stopLane(downloadLane_);
        stopLane(loadModelLane_);
    }

    ModelManager(ModelManager const &) = delete;
    ModelManager &operator=(ModelManager const &) = delete;

    std::uint64_t refreshLocalModels() override {
        std::uint64_t const requestId = beginRequest(ModelManagerLane::Inventory);
        enqueueTask(inventoryLane_, [this, requestId] { postLocalModelsReady_(requestId); });
        return requestId;
    }

    std::uint64_t searchHuggingFace(HfSearchRequest request) override {
        std::uint64_t const requestId = beginRequest(ModelManagerLane::Search);
        enqueueTask(searchLane_, [this, requestId, req = std::move(request)] {
            try {
                auto [results, rawJson] = searchHfSync(req);
                postEvent(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfSearchReady,
                    .hfModels = std::move(results),
                    .searchKey = req.cacheKey,
                    .rawJson = std::move(rawJson),
                },
                          ModelManagerLane::Search,
                          requestId);
            } catch (std::exception const &e) {
                postEvent(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfSearchReady,
                    .error = e.what(),
                    .searchKey = req.cacheKey,
                },
                          ModelManagerLane::Search,
                          requestId);
            }
        });
        return requestId;
    }

    std::uint64_t listRepoFiles(std::string repoId) override {
        std::uint64_t const requestId = beginRequest(ModelManagerLane::RepoInspect);
        enqueueTask(repoInspectLane_, [this, requestId, repo = std::move(repoId)] {
            try {
                auto response = listRepoFilesSync(repo);
                postEvent(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfFilesReady,
                    .hfFiles = std::move(response.files),
                    .repoId = repo,
                    .rawJson = std::move(response.rawJson),
                },
                          ModelManagerLane::RepoInspect,
                          requestId);
            } catch (std::exception const &e) {
                postEvent(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfFilesReady,
                    .error = e.what(),
                    .repoId = repo,
                },
                          ModelManagerLane::RepoInspect,
                          requestId);
            }
        });
        return requestId;
    }

    std::uint64_t fetchRepoDetail(std::string repoId) override {
        std::uint64_t const requestId = beginRequest(ModelManagerLane::RepoInspect);
        enqueueTask(repoInspectLane_, [this, requestId, repo = std::move(repoId)] {
            try {
                auto response = fetchRepoDetailSync(repo);
                postEvent(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfRepoDetailReady,
                    .hfRepoDetail = std::move(response.detail),
                    .repoId = repo,
                    .rawJson = std::move(response.rawJson),
                },
                          ModelManagerLane::RepoInspect,
                          requestId);
            } catch (std::exception const &e) {
                postEvent(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfRepoDetailReady,
                    .error = e.what(),
                    .repoId = repo,
                },
                          ModelManagerLane::RepoInspect,
                          requestId);
            }
        });
        return requestId;
    }

    std::uint64_t inspectRepo(std::string repoId) override {
        std::uint64_t const requestId = beginRequest(ModelManagerLane::RepoInspect);
        enqueueTask(repoInspectLane_, [this, requestId, repo = std::move(repoId)] {
            try {
                auto detailResponse = fetchRepoDetailSync(repo);
                postEvent(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfRepoDetailReady,
                    .hfRepoDetail = std::move(detailResponse.detail),
                    .repoId = repo,
                    .rawJson = std::move(detailResponse.rawJson),
                },
                          ModelManagerLane::RepoInspect,
                          requestId);
            } catch (std::exception const &e) {
                postEvent(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfRepoDetailReady,
                    .error = e.what(),
                    .repoId = repo,
                },
                          ModelManagerLane::RepoInspect,
                          requestId);
            }

            try {
                auto filesResponse = listRepoFilesSync(repo);
                postEvent(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfFilesReady,
                    .hfFiles = std::move(filesResponse.files),
                    .repoId = repo,
                    .rawJson = std::move(filesResponse.rawJson),
                },
                          ModelManagerLane::RepoInspect,
                          requestId);
            } catch (std::exception const &e) {
                postEvent(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::HfFilesReady,
                    .error = e.what(),
                    .repoId = repo,
                },
                          ModelManagerLane::RepoInspect,
                          requestId);
            }
        });
        return requestId;
    }

    std::uint64_t downloadModel(std::string repoId, std::string fileName) override {
        std::uint64_t const requestId = beginRequest(ModelManagerLane::Download);
        latestDownloadRequestId_.store(requestId, std::memory_order_relaxed);
        downloadCancelRequested_.store(false, std::memory_order_relaxed);
        enqueueTask(downloadLane_, [this, requestId, repo = std::move(repoId), file = std::move(fileName)] {
            try {
                if (isDownloadCancelled(requestId)) {
                    return;
                }
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
                    std::uint64_t requestId = 0;
                    std::chrono::steady_clock::time_point lastPostedAt {};
                };

                DownloadProgressContext progressContext {
                    .manager = this,
                    .repoId = repo,
                    .filePath = file,
                    .requestId = requestId,
                };

                struct DownloadProgressCallback : common_download_callback {
                    explicit DownloadProgressCallback(DownloadProgressContext &context) : ctx(context) {}

                    void on_start(common_download_progress const &progress) override { update(progress); }

                    void on_update(common_download_progress const &progress) override { update(progress); }

                    void on_done(common_download_progress const &progress, bool ok) override {
                        update(progress, true);
                        if (!ok || ctx.manager == nullptr) {
                            return;
                        }

                        std::lock_guard<std::mutex> lock(ctx.mutex);
                        ctx.lastDownloadedBytes = std::max(ctx.lastDownloadedBytes, progress.downloaded);
                        ctx.lastTotalBytes = std::max(ctx.lastTotalBytes, progress.total);
                    }

                    void update(common_download_progress const &progress, bool force = false) {
                        if (ctx.manager == nullptr || progress.total == 0 || ctx.manager->isDownloadCancelled(ctx.requestId)) {
                            return;
                        }

                        using clock = std::chrono::steady_clock;
                        auto const now = clock::now();
                        int const percent = static_cast<int>(
                            (100.0 * static_cast<double>(progress.downloaded)) / static_cast<double>(progress.total)
                        );

                        bool shouldPost = false;
                        {
                            std::lock_guard<std::mutex> lock(ctx.mutex);
                            shouldPost =
                                force ||
                                progress.downloaded >= progress.total ||
                                ctx.lastPercent < 0 ||
                                percent >= ctx.lastPercent + 1 ||
                                now - ctx.lastPostedAt >= std::chrono::milliseconds(120);
                            if (!shouldPost && progress.downloaded <= ctx.lastDownloadedBytes) {
                                return;
                            }
                            ctx.lastDownloadedBytes = std::max(ctx.lastDownloadedBytes, progress.downloaded);
                            ctx.lastTotalBytes = std::max(ctx.lastTotalBytes, progress.total);
                            if (!shouldPost) {
                                return;
                            }
                            ctx.lastPercent = percent;
                            ctx.lastPostedAt = now;
                        }

                        ctx.manager->postEvent(ModelManagerEvent {
                            .kind = ModelManagerEvent::Kind::DownloadProgress,
                            .repoId = ctx.repoId,
                            .filePath = ctx.filePath,
                            .downloadedBytes = progress.downloaded,
                            .totalBytes = progress.total,
                        },
                                             ModelManagerLane::Download,
                                             ctx.requestId);
                    }

                    bool is_cancelled() const override {
                        return ctx.manager == nullptr || ctx.manager->isDownloadCancelled(ctx.requestId);
                    }

                    DownloadProgressContext &ctx;
                };

                common_download_opts opts;
                opts.bearer_token = token;
                DownloadProgressCallback callback(progressContext);
                opts.callback = &callback;

                auto result = common_download_model(model, opts);
                if (isDownloadCancelled(requestId)) {
                    return;
                }
                if (result.model_path.empty()) {
                    postEvent(ModelManagerEvent {
                        .kind = ModelManagerEvent::Kind::DownloadError,
                        .error = "Download returned empty path",
                        .repoId = repo,
                        .filePath = file,
                    },
                              ModelManagerLane::Download,
                              requestId);
                    return;
                }

                postEvent(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::DownloadDone,
                    .repoId = repo,
                    .filePath = file,
                    .modelPath = result.model_path,
                    .modelName = repo + "/" + file,
                    .downloadedBytes = progressContext.lastDownloadedBytes,
                    .totalBytes = progressContext.lastTotalBytes,
                },
                          ModelManagerLane::Download,
                          requestId);

                std::uint64_t const refreshRequestId = beginRequest(ModelManagerLane::Inventory);
                enqueueTask(inventoryLane_, [this, refreshRequestId] { postLocalModelsReady_(refreshRequestId); });
            } catch (std::exception const &e) {
                if (isDownloadCancelled(requestId)) {
                    return;
                }
                postEvent(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::DownloadError,
                    .error = e.what(),
                    .repoId = repo,
                    .filePath = file,
                },
                          ModelManagerLane::Download,
                          requestId);
            }
        });
        return requestId;
    }

    std::uint64_t cancelDownload() override {
        downloadCancelRequested_.store(true, std::memory_order_relaxed);
        std::uint64_t const requestId = nextRequestId_.fetch_add(1, std::memory_order_relaxed);
        latestDownloadRequestId_.store(requestId, std::memory_order_relaxed);
        return requestId;
    }

    std::uint64_t loadModel(LoadParams params) override {
        std::uint64_t const requestId = beginRequest(ModelManagerLane::LoadModel);
        enqueueTask(loadModelLane_, [this, requestId, params = std::move(params)] {
            bool const ok = engine_->load(params);
            if (ok) {
                postEvent(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::ModelLoaded,
                    .modelPath = params.modelPath,
                    .modelName = extractModelName(params.modelPath),
                    .appliedLoadParams = params,
                },
                          ModelManagerLane::LoadModel,
                          requestId);
            } else {
                postEvent(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::ModelLoadError,
                    .error = std::string("Failed to load: ") + params.modelPath,
                    .appliedLoadParams = params,
                },
                          ModelManagerLane::LoadModel,
                          requestId);
            }
        });
        return requestId;
    }

    std::uint64_t deleteModel(std::string path, std::string repoId) override {
        std::uint64_t const requestId = beginRequest(ModelManagerLane::Download);
        enqueueTask(downloadLane_, [this, requestId, modelPath = std::move(path), repo = std::move(repoId)] {
            try {
                if (modelPath.empty()) {
                    throw std::runtime_error("Model path is empty");
                }

                if (pathsEquivalent(engine_->modelPath(), modelPath)) {
                    engine_->cancelAllGenerations();
                    engine_->unload();
                }

                bool removedArtifacts = false;
                std::string deletedTarget;
                if (std::optional<fs::path> repoPath = resolveHfRepoPath(repo, modelPath); repoPath.has_value()) {
                    std::error_code ec;
                    if (fs::exists(*repoPath, ec) && !ec) {
                        fs::remove_all(*repoPath, ec);
                        if (ec) {
                            throw std::runtime_error("Failed to remove Hugging Face cache repo: " + ec.message());
                        }
                        removedArtifacts = true;
                        deletedTarget = repoPath->string();
                    }
                }

                if (!removedArtifacts) {
                    fs::path const filePath = fs::path(modelPath).lexically_normal();
                    std::error_code ec;
                    if (fs::exists(filePath, ec) && !ec) {
                        if (fs::is_directory(filePath, ec)) {
                            fs::remove_all(filePath, ec);
                        } else {
                            fs::remove(filePath, ec);
                        }
                        if (ec) {
                            throw std::runtime_error("Failed to remove model file: " + ec.message());
                        }
                        removedArtifacts = true;
                        deletedTarget = filePath.string();
                    }
                }

                if (!removedArtifacts) {
                    throw std::runtime_error("Model artifacts were not found");
                }

                postEvent(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::ModelDeleted,
                    .repoId = repo,
                    .filePath = deletedTarget,
                    .modelPath = modelPath,
                    .modelName = extractModelName(modelPath),
                },
                          ModelManagerLane::Download,
                          requestId);

                std::uint64_t const refreshRequestId = beginRequest(ModelManagerLane::Inventory);
                enqueueTask(inventoryLane_, [this, refreshRequestId] { postLocalModelsReady_(refreshRequestId); });
            } catch (std::exception const &e) {
                postEvent(ModelManagerEvent {
                    .kind = ModelManagerEvent::Kind::ModelDeleteError,
                    .error = e.what(),
                    .repoId = repo,
                    .modelPath = modelPath,
                },
                          ModelManagerLane::Download,
                          requestId);
            }
        });
        return requestId;
    }

    void unloadModel() override {
        engine_->cancelAllGenerations();
        engine_->unload();
    }

  private:
    using LaneTask = std::function<void()>;

    struct LaneState {
        explicit LaneState(bool latestWins) : latestWins(latestWins) {}

        bool latestWins = false;
        std::mutex mutex;
        std::condition_variable cv;
        std::deque<LaneTask> tasks;
        bool stopping = false;
        std::thread worker;
    };

    static bool isLatestWinsLane(ModelManagerLane lane) {
        return lane == ModelManagerLane::Inventory || lane == ModelManagerLane::Search ||
               lane == ModelManagerLane::RepoInspect;
    }

    std::uint64_t beginRequest(ModelManagerLane lane) {
        std::uint64_t const requestId = nextRequestId_.fetch_add(1, std::memory_order_relaxed);
        if (!isLatestWinsLane(lane)) {
            return requestId;
        }
        switch (lane) {
        case ModelManagerLane::Inventory:
            latestInventoryRequestId_.store(requestId, std::memory_order_relaxed);
            break;
        case ModelManagerLane::Search:
            latestSearchRequestId_.store(requestId, std::memory_order_relaxed);
            break;
        case ModelManagerLane::RepoInspect:
            latestRepoInspectRequestId_.store(requestId, std::memory_order_relaxed);
            break;
        case ModelManagerLane::Download:
        case ModelManagerLane::LoadModel:
            break;
        }
        return requestId;
    }

    bool isCurrentRequest(ModelManagerLane lane, std::uint64_t requestId) const {
        if (!isLatestWinsLane(lane)) {
            return true;
        }
        switch (lane) {
        case ModelManagerLane::Inventory:
            return latestInventoryRequestId_.load(std::memory_order_relaxed) == requestId;
        case ModelManagerLane::Search:
            return latestSearchRequestId_.load(std::memory_order_relaxed) == requestId;
        case ModelManagerLane::RepoInspect:
            return latestRepoInspectRequestId_.load(std::memory_order_relaxed) == requestId;
        case ModelManagerLane::Download:
        case ModelManagerLane::LoadModel:
            return true;
        }
        return true;
    }

    bool isDownloadCancelled(std::uint64_t requestId) const {
        return shuttingDown_.load(std::memory_order_relaxed) ||
               downloadCancelRequested_.load(std::memory_order_relaxed) ||
               latestDownloadRequestId_.load(std::memory_order_relaxed) != requestId;
    }

    void startLane(LaneState &lane) {
        lane.worker = std::thread([&lane]() {
            while (true) {
                LaneTask task;
                {
                    std::unique_lock<std::mutex> lock(lane.mutex);
                    lane.cv.wait(lock, [&lane]() { return lane.stopping || !lane.tasks.empty(); });
                    if (lane.stopping && lane.tasks.empty()) {
                        return;
                    }
                    task = std::move(lane.tasks.front());
                    lane.tasks.pop_front();
                }
                task();
            }
        });
    }

    void stopLane(LaneState &lane) {
        {
            std::lock_guard<std::mutex> lock(lane.mutex);
            lane.stopping = true;
            lane.cv.notify_all();
        }
        if (lane.worker.joinable()) {
            lane.worker.join();
        }
    }

    void enqueueTask(LaneState &lane, LaneTask task) {
        std::lock_guard<std::mutex> lock(lane.mutex);
        if (lane.latestWins) {
            lane.tasks.clear();
        }
        lane.tasks.push_back(std::move(task));
        lane.cv.notify_one();
    }

    void postEvent(ModelManagerEvent event, ModelManagerLane lane, std::uint64_t requestId) {
        if (!isCurrentRequest(lane, requestId)) {
            return;
        }
        event.requestId = requestId;
        event.lane = lane;
        post_(std::move(event));
    }

    void postLocalModelsReady_(std::uint64_t requestId) {
        auto models = listLocalModelsSync();
        postEvent(ModelManagerEvent {
            .kind = ModelManagerEvent::Kind::LocalModelsReady,
            .localModels = std::move(models),
        },
                  ModelManagerLane::Inventory,
                  requestId);
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
        std::string url = common_get_model_endpoint();
        url += "api/models?library=gguf&direction=-1&limit=20&full=true&cardData=true";
        if (!request.sortKey.empty()) {
            url += "&sort=" + urlEncode(request.sortKey);
        }
        if (!request.query.empty()) {
            url += "&search=" + urlEncode(request.query);
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
        std::string url = common_get_model_endpoint() + "api/models/" + repoId;
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
        std::string const readmeUrl = common_get_model_endpoint() + repoId + "/resolve/main/README.md";
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
        std::string url = common_get_model_endpoint() + "api/models/" + repoId + "/refs";
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
        std::string url = common_get_model_endpoint() + "api/models/" + repoId + "/tree/" + commit + "?recursive=true";
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

    static std::optional<fs::path> hfCacheDirectory() {
        if (char const *cache = std::getenv("LLAMA_CACHE"); cache != nullptr && *cache) {
            return fs::path(cache);
        }
        if (char const *cache = std::getenv("HF_HUB_CACHE"); cache != nullptr && *cache) {
            return fs::path(cache);
        }
        if (char const *cache = std::getenv("HUGGINGFACE_HUB_CACHE"); cache != nullptr && *cache) {
            return fs::path(cache);
        }
        if (char const *home = std::getenv("HF_HOME"); home != nullptr && *home) {
            return fs::path(home) / "hub";
        }
        if (char const *xdg = std::getenv("XDG_CACHE_HOME"); xdg != nullptr && *xdg) {
            return fs::path(xdg) / "huggingface" / "hub";
        }
        if (char const *home = std::getenv("HOME"); home != nullptr && *home) {
            return fs::path(home) / ".cache" / "huggingface" / "hub";
        }
        return std::nullopt;
    }

    static bool isValidRepoId(std::string const &repoId) {
        if (repoId.empty() || repoId.size() > 256) {
            return false;
        }

        int slashCount = 0;
        bool previousWasSpecial = true;
        for (char const ch : repoId) {
            bool const isBase = std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
            bool const isSpecial = ch == '/' || ch == '.' || ch == '-';
            if (isBase) {
                previousWasSpecial = false;
                continue;
            }
            if (!isSpecial || previousWasSpecial) {
                return false;
            }
            if (ch == '/') {
                ++slashCount;
            }
            previousWasSpecial = true;
        }
        return !previousWasSpecial && slashCount == 1;
    }

    static fs::path absoluteNormalized(fs::path const &path) {
        std::error_code ec;
        fs::path absolutePath = fs::absolute(path, ec);
        if (ec) {
            return path.lexically_normal();
        }
        return absolutePath.lexically_normal();
    }

    static bool isSubpath(fs::path const &root, fs::path const &path) {
        fs::path const normalizedRoot = absoluteNormalized(root);
        fs::path const normalizedPath = absoluteNormalized(path);
        auto mismatch = std::mismatch(
            normalizedRoot.begin(),
            normalizedRoot.end(),
            normalizedPath.begin(),
            normalizedPath.end()
        );
        return mismatch.first == normalizedRoot.end();
    }

    static std::optional<fs::path> repoPathFromRepoId(std::string const &repoId) {
        if (!isValidRepoId(repoId)) {
            return std::nullopt;
        }
        std::optional<fs::path> const cacheDir = hfCacheDirectory();
        if (!cacheDir.has_value()) {
            return std::nullopt;
        }

        // Repo folder uses "--" to represent "/".
        std::string encoded = "models--" + repoId;
        std::string::size_type pos = 0;
        while ((pos = encoded.find('/', pos)) != std::string::npos) {
            encoded.replace(pos, 1, "--");
            pos += 2;
        }

        fs::path repoPath = *cacheDir / encoded;
        if (!isSubpath(*cacheDir, repoPath)) {
            return std::nullopt;
        }
        return repoPath;
    }

    static std::optional<fs::path> repoPathFromModelPath(std::string const &modelPath) {
        std::optional<fs::path> const cacheDir = hfCacheDirectory();
        if (!cacheDir.has_value()) {
            return std::nullopt;
        }

        fs::path current = fs::path(modelPath).lexically_normal().parent_path();
        while (!current.empty()) {
            std::string const name = current.filename().string();
            if (name.rfind("models--", 0) == 0 && isSubpath(*cacheDir, current)) {
                return current;
            }

            fs::path const parent = current.parent_path();
            if (parent == current) {
                break;
            }
            current = parent;
        }
        return std::nullopt;
    }

    static std::optional<fs::path> resolveHfRepoPath(std::string const &repoId, std::string const &modelPath) {
        if (!repoId.empty()) {
            if (std::optional<fs::path> fromRepo = repoPathFromRepoId(repoId); fromRepo.has_value()) {
                return fromRepo;
            }
        }
        return repoPathFromModelPath(modelPath);
    }

    static bool pathsEquivalent(std::string const &lhs, std::string const &rhs) {
        if (lhs.empty() || rhs.empty()) {
            return false;
        }

        std::error_code ec;
        if (fs::equivalent(lhs, rhs, ec) && !ec) {
            return true;
        }
        return fs::path(lhs).lexically_normal() == fs::path(rhs).lexically_normal();
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

    std::shared_ptr<lambda::IChatEngine> engine_;
    PostFn post_;
    std::atomic<std::uint64_t> nextRequestId_ {1};
    std::atomic<std::uint64_t> latestInventoryRequestId_ {0};
    std::atomic<std::uint64_t> latestSearchRequestId_ {0};
    std::atomic<std::uint64_t> latestRepoInspectRequestId_ {0};
    std::atomic<std::uint64_t> latestDownloadRequestId_ {0};
    std::atomic<bool> downloadCancelRequested_ {false};
    std::atomic<bool> shuttingDown_ {false};
    LaneState inventoryLane_;
    LaneState searchLane_;
    LaneState repoInspectLane_;
    LaneState downloadLane_;
    LaneState loadModelLane_;
};

} // namespace lambda_studio_backend
