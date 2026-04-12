#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace lambda_backend {

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

struct HfFileInfo {
    std::string repoId;
    std::string path;
    std::size_t sizeBytes = 0;
    std::string localPath;
    bool cached = false;

    constexpr bool operator==(HfFileInfo const &) const = default;
};

struct ModelManagerEvent {
    enum class Kind {
        LocalModelsReady,
        HfSearchReady,
        HfFilesReady,
        DownloadDone,
        DownloadError,
        ModelLoaded,
        ModelLoadError,
    };

    Kind kind = Kind::LocalModelsReady;
    std::vector<LocalModelInfo> localModels;
    std::vector<HfModelInfo> hfModels;
    std::vector<HfFileInfo> hfFiles;
    std::string error;
    std::string modelPath;
    std::string modelName;
    std::string rawJson;
};

struct SamplingParams {
    float temp = 0.80f;
    float topP = 0.95f;
    int32_t topK = 40;
    int32_t maxTokens = 4096;
    int32_t nGpuLayers = -1;

    constexpr bool operator==(SamplingParams const &) const = default;
};

} // namespace lambda_backend
