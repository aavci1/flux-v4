#pragma once

/// \file ModelManager.hpp
///
/// Orchestrates model listing (local + Hugging Face), downloading, and
/// loading/unloading. All blocking operations run on a background thread;
/// results are posted back to the Flux event queue as ModelManagerEvent.

#include "common.h"
#include "download.h"
#include "hf-cache.h"

#define JSON_ASSERT GGML_ASSERT
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "LlamaEngine.hpp"
#include "Types.hpp"

namespace llm_studio {

namespace fs = std::filesystem;

class ModelManager {
public:
    using PostFn = std::function<void(ModelManagerEvent)>;

    explicit ModelManager(std::shared_ptr<LlamaEngine> engine, PostFn post)
        : engine_(std::move(engine)), post_(std::move(post)) {}

    ~ModelManager() { joinWorker(); }

    ModelManager(ModelManager const&) = delete;
    ModelManager& operator=(ModelManager const&) = delete;

    // ── List locally cached models (background) ─────────────────────────

    void refreshLocalModels() {
        runAsync([this] { postLocalModelsReady_(); });
    }

    // ── Search Hugging Face for GGUF models (background) ────────────────

    void searchHuggingFace(std::string query) {
        runAsync([this, q = std::move(query)] {
            try {
                auto results = searchHfSync(q);
                post_(ModelManagerEvent{
                    .kind = ModelManagerEvent::Kind::HfSearchReady,
                    .hfModels = std::move(results),
                });
            } catch (std::exception const& e) {
                post_(ModelManagerEvent{
                    .kind = ModelManagerEvent::Kind::HfSearchReady,
                    .error = e.what(),
                });
            }
        });
    }

    // ── List GGUF files in a specific HF repo (background) ──────────────

    void listRepoFiles(std::string repoId) {
        runAsync([this, repo = std::move(repoId)] {
            try {
                auto files = listRepoFilesSync(repo);
                post_(ModelManagerEvent{
                    .kind = ModelManagerEvent::Kind::HfFilesReady,
                    .hfFiles = std::move(files),
                });
            } catch (std::exception const& e) {
                post_(ModelManagerEvent{
                    .kind = ModelManagerEvent::Kind::HfFilesReady,
                    .error = e.what(),
                });
            }
        });
    }

    // ── Download a specific file from HF (background) ───────────────────

    void downloadModel(std::string repoId, std::string fileName) {
        runAsync([this, repo = std::move(repoId), file = std::move(fileName)] {
            try {
                common_params_model m;
                m.hf_repo = repo;
                m.hf_file = file;

                std::string token = hfToken();
                auto result = common_download_model(m, token);

                if (result.model_path.empty()) {
                    post_(ModelManagerEvent{
                        .kind = ModelManagerEvent::Kind::DownloadError,
                        .error = "Download returned empty path",
                    });
                    return;
                }

                post_(ModelManagerEvent{
                    .kind = ModelManagerEvent::Kind::DownloadDone,
                    .modelPath = result.model_path,
                    .modelName = repo + "/" + file,
                });

                postLocalModelsReady_();
            } catch (std::exception const& e) {
                post_(ModelManagerEvent{
                    .kind = ModelManagerEvent::Kind::DownloadError,
                    .error = e.what(),
                });
            }
        });
    }

    // ── Load a model from a local path (background) ─────────────────────

    void loadModel(std::string path, int nGpuLayers = -1) {
        runAsync([this, p = std::move(path), gpu = nGpuLayers] {
            bool ok = engine_->load(p, gpu);
            if (ok) {
                auto name = extractModelName(p);
                post_(ModelManagerEvent{
                    .kind = ModelManagerEvent::Kind::ModelLoaded,
                    .modelPath = p,
                    .modelName = name,
                });
            } else {
                post_(ModelManagerEvent{
                    .kind = ModelManagerEvent::Kind::ModelLoadError,
                    .error = std::string("Failed to load: ") + p,
                });
            }
        });
    }

    void unloadModel() {
        engine_->cancelGeneration();
        engine_->unload();
    }

private:
    /// Scan disk and post LocalModelsReady (caller may already be on the worker thread).
    void postLocalModelsReady_() {
        auto models = listLocalModelsSync();
        post_(ModelManagerEvent{
            .kind = ModelManagerEvent::Kind::LocalModelsReady,
            .localModels = std::move(models),
        });
    }

    void joinWorker() {
        if (worker_.joinable()) worker_.join();
    }

    void runAsync(std::function<void()> fn) {
        joinWorker();
        worker_ = std::thread(std::move(fn));
    }

    static std::string hfToken() {
        if (auto* t = std::getenv("HF_TOKEN")) return t;
        if (auto* t = std::getenv("HUGGING_FACE_HUB_TOKEN")) return t;

        std::string tokenPath;
        if (auto* h = std::getenv("HF_HOME")) {
            tokenPath = std::string(h) + "/token";
        } else if (auto* home = std::getenv("HOME")) {
            tokenPath = std::string(home) + "/.cache/huggingface/token";
        }
        if (!tokenPath.empty() && fs::exists(tokenPath)) {
            std::ifstream f(tokenPath);
            std::string token;
            if (std::getline(f, token) && !token.empty()) return token;
        }
        return {};
    }

    // ── Synchronous helpers (called from worker thread) ─────────────────

    std::vector<LocalModelInfo> listLocalModelsSync() const {
        std::vector<LocalModelInfo> result;

        auto cached = common_list_cached_models();
        for (auto const& m : cached) {
            result.push_back(LocalModelInfo{
                .repo = m.repo,
                .tag  = m.tag,
            });
        }

        scanDirectory(modelsDir(), result);

        return result;
    }

    std::vector<HfModelInfo> searchHfSync(std::string const& query) const {
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

        if (!json.is_array()) return results;

        for (auto const& item : json) {
            HfModelInfo info;
            if (item.contains("id") && item["id"].is_string())
                info.id = item["id"].get<std::string>();
            if (item.contains("downloads") && item["downloads"].is_number())
                info.downloads = item["downloads"].get<int64_t>();
            if (item.contains("likes") && item["likes"].is_number())
                info.likes = item["likes"].get<int64_t>();
            if (item.contains("pipeline_tag") && item["pipeline_tag"].is_string())
                info.pipelineTag = item["pipeline_tag"].get<std::string>();
            if (!info.id.empty())
                results.push_back(std::move(info));
        }
        return results;
    }

    std::vector<HfFileInfo> listRepoFilesSync(std::string const& repoId) const {
        std::string token = hfToken();
        auto allFiles = hf_cache::get_repo_files(repoId, token);

        std::vector<HfFileInfo> ggufFiles;
        for (auto const& f : allFiles) {
            if (f.path.size() >= 5 && f.path.substr(f.path.size() - 5) == ".gguf") {
                ggufFiles.push_back(HfFileInfo{
                    .repoId   = repoId,
                    .path     = f.path,
                    .sizeBytes = f.size,
                });
            }
        }
        return ggufFiles;
    }

    static void scanDirectory(std::string const& dir, std::vector<LocalModelInfo>& out) {
        if (dir.empty() || !fs::is_directory(dir)) return;
        try {
            for (auto const& entry : fs::recursive_directory_iterator(dir)) {
                if (!entry.is_regular_file()) continue;
                auto p = entry.path().string();
                if (p.size() >= 5 && p.substr(p.size() - 5) == ".gguf") {
                    out.push_back(LocalModelInfo{
                        .path = p,
                        .sizeBytes = static_cast<size_t>(entry.file_size()),
                    });
                }
            }
        } catch (...) {}
    }

    static std::string modelsDir() {
        if (auto* home = std::getenv("HOME"))
            return std::string(home) + "/.llm-studio/models";
        return {};
    }

    static std::string extractModelName(std::string const& path) {
        auto pos = path.rfind('/');
        std::string name = (pos != std::string::npos) ? path.substr(pos + 1) : path;
        if (name.size() > 5 && name.substr(name.size() - 5) == ".gguf")
            name = name.substr(0, name.size() - 5);
        return name;
    }

    static std::string urlEncode(std::string const& s) {
        std::string result;
        for (unsigned char c : s) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                result += static_cast<char>(c);
            } else {
                char buf[4];
                std::snprintf(buf, sizeof(buf), "%%%02X", c);
                result += buf;
            }
        }
        return result;
    }

    static std::string formatSize(size_t bytes) {
        if (bytes >= 1024ULL * 1024 * 1024)
            return string_format("%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
        if (bytes >= 1024ULL * 1024)
            return string_format("%.1f MB", bytes / (1024.0 * 1024.0));
        return string_format("%zu B", bytes);
    }

    std::shared_ptr<LlamaEngine> engine_;
    PostFn                       post_;
    std::thread                  worker_;
};

} // namespace llm_studio
