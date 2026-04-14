#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace lambda_studio_backend {

struct ChatMessage {
    enum class Role {
        User,
        Reasoning,
        Assistant,
    };

    Role role = Role::User;
    std::string text;

    constexpr bool operator==(ChatMessage const &) const = default;
};

struct GenerationStats {
    std::int64_t promptTokens = 0;
    std::int64_t completionTokens = 0;
    std::int64_t startedAtUnixMs = 0;
    std::int64_t firstTokenAtUnixMs = 0;
    std::int64_t finishedAtUnixMs = 0;
    double tokensPerSecond = 0.0;
    std::string status;
    std::string errorText;
    float temp = 0.f;
    float topP = 0.f;
    std::int32_t topK = 0;
    std::int32_t maxTokens = 0;

    constexpr bool operator==(GenerationStats const &) const = default;
};

struct LocalModelInfo {
    std::string repo;
    std::string tag;
    std::string path;
    std::size_t sizeBytes = 0;

    std::string displayName() const {
        if (!path.empty()) {
            std::filesystem::path filePath(path);
            std::string name = filePath.filename().string();
            if (name.size() > 5 && name.substr(name.size() - 5) == ".gguf") {
                name.resize(name.size() - 5);
            }
            if (!name.empty()) {
                return name;
            }
        }
        if (!repo.empty()) {
            return tag.empty() ? repo : repo + ":" + tag;
        }
        auto const pos = path.rfind('/');
        return pos != std::string::npos ? path.substr(pos + 1) : path;
    }

    constexpr bool operator==(LocalModelInfo const &) const = default;
};

struct HfModelInfo {
    std::string id;
    std::string author;
    std::string libraryName;
    std::string lastModified;
    std::string createdAt;
    std::int64_t downloads = 0;
    std::int64_t downloadsAllTime = 0;
    std::int64_t likes = 0;
    std::int64_t usedStorage = 0;
    std::string pipelineTag;
    std::vector<std::string> tags;
    bool gated = false;
    bool isPrivate = false;
    bool disabled = false;

    constexpr bool operator==(HfModelInfo const &) const = default;
};

struct HfSearchRequest {
    std::string query;
    std::string author;
    std::string sortKey = "downloads";
    std::string visibilityFilter = "all";
    std::string cacheKey;

    constexpr bool operator==(HfSearchRequest const &) const = default;
};

struct HfFileInfo {
    std::string repoId;
    std::string path;
    std::size_t sizeBytes = 0;
    std::string localPath;
    bool cached = false;

    constexpr bool operator==(HfFileInfo const &) const = default;
};

struct HfRepoDetailInfo {
    std::string id;
    std::string author;
    std::string sha;
    std::string libraryName;
    std::string pipelineTag;
    std::string license;
    std::string summary;
    std::string readme;
    std::string createdAt;
    std::string lastModified;
    std::vector<std::string> tags;
    std::vector<std::string> languages;
    std::vector<std::string> baseModels;
    std::int64_t downloads = 0;
    std::int64_t downloadsAllTime = 0;
    std::int64_t likes = 0;
    std::int64_t usedStorage = 0;
    bool gated = false;
    bool isPrivate = false;
    bool disabled = false;

    constexpr bool operator==(HfRepoDetailInfo const &) const = default;
};

enum class ModelManagerLane {
    Inventory,
    Search,
    RepoInspect,
    Download,
    LoadModel,
};

struct ModelManagerEvent {
    enum class Kind {
        LocalModelsReady,
        HfSearchReady,
        HfFilesReady,
        HfRepoDetailReady,
        DownloadProgress,
        DownloadDone,
        DownloadError,
        ModelLoaded,
        ModelLoadError,
        ModelDeleted,
        ModelDeleteError,
    };

    Kind kind = Kind::LocalModelsReady;
    std::vector<LocalModelInfo> localModels;
    std::vector<HfModelInfo> hfModels;
    std::vector<HfFileInfo> hfFiles;
    HfRepoDetailInfo hfRepoDetail;
    std::string error;
    std::string searchKey;
    std::string repoId;
    std::string filePath;
    std::string modelPath;
    std::string modelName;
    std::string rawJson;
    std::size_t downloadedBytes = 0;
    std::size_t totalBytes = 0;
    std::uint64_t requestId = 0;
    ModelManagerLane lane = ModelManagerLane::Inventory;
};

struct SamplingParams {
    float temp = 0.80f;
    float topP = 0.95f;
    int32_t topK = 40;
    int32_t maxTokens = 4096;
    int32_t nGpuLayers = -1;

    constexpr bool operator==(SamplingParams const &) const = default;
};

} // namespace lambda_studio_backend
