#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ChatModels.hpp"

namespace lambda {

enum class StudioModule {
    Chats,
    Models,
    Hub,
    Settings,
};

enum class RemoteModelSort {
    Downloads,
    Likes,
    Updated,
};

enum class RemoteModelVisibilityFilter {
    All,
    PublicOnly,
    GatedOnly,
};

enum class DownloadJobStatus {
    Running,
    Completed,
    Failed,
};

struct AppNotice {
    std::string title;
    std::string detail;
    StudioModule targetModule = StudioModule::Models;

    bool operator==(AppNotice const &) const = default;
};

struct LocalModel {
    std::string path;
    std::string name;
    std::string repo;
    std::string tag;
    std::size_t sizeBytes = 0;

    bool operator==(LocalModel const &) const = default;
};

struct RemoteModel {
    std::string id;
    std::string author;
    std::string libraryName;
    std::string pipelineTag;
    std::string createdAt;
    std::string lastModified;
    std::vector<std::string> tags;
    std::int64_t downloads = 0;
    std::int64_t downloadsAllTime = 0;
    std::int64_t likes = 0;
    std::int64_t usedStorage = 0;
    bool gated = false;
    bool isPrivate = false;
    bool disabled = false;

    bool operator==(RemoteModel const &) const = default;
};

struct RemoteModelFile {
    std::string repoId;
    std::string path;
    std::string localPath;
    std::size_t sizeBytes = 0;
    bool cached = false;

    bool operator==(RemoteModelFile const &) const = default;
};

struct RemoteRepoDetail {
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

    bool operator==(RemoteRepoDetail const &) const = default;
};

struct DownloadJob {
    std::string id;
    std::string repoId;
    std::string filePath;
    std::string localPath;
    std::string error;
    std::int64_t startedAtUnixMs = 0;
    std::int64_t finishedAtUnixMs = 0;
    DownloadJobStatus status = DownloadJobStatus::Running;
    std::size_t downloadedBytes = 0;
    std::size_t totalBytes = 0;

    bool operator==(DownloadJob const &) const = default;
};

struct AppState {
    StudioModule currentModule = StudioModule::Chats;
    std::vector<ChatThread> chats = sampleChatThreads();
    int selectedChatIndex = 0;

    std::vector<LocalModel> localModels;
    std::string modelSearchQuery;
    std::string modelSearchAuthor;
    RemoteModelSort remoteModelSort = RemoteModelSort::Downloads;
    RemoteModelVisibilityFilter remoteModelVisibility = RemoteModelVisibilityFilter::All;
    std::vector<RemoteModel> remoteModels;
    std::string selectedRemoteRepoId;
    std::vector<RemoteModelFile> selectedRemoteRepoFiles;
    std::optional<RemoteRepoDetail> selectedRemoteRepoDetail;
    std::vector<DownloadJob> recentDownloadJobs;

    std::string loadedModelPath;
    std::string loadedModelName;
    std::string pendingModelPath;
    std::string pendingModelName;

    std::optional<AppNotice> notice;
    std::string statusText;
    std::string errorText;

    bool refreshingModels = false;
    bool searchingRemoteModels = false;
    bool loadingRemoteModelFiles = false;
    bool loadingRemoteRepoDetail = false;
    bool downloadingModel = false;
    bool modelLoading = false;

    std::string pendingDownloadJobId;
    std::string pendingDownloadRepoId;
    std::string pendingDownloadFilePath;

    bool operator==(AppState const &) const = default;
};

inline char const *moduleTitle(StudioModule module) {
    switch (module) {
    case StudioModule::Chats:
        return "Chats";
    case StudioModule::Models:
        return "Models";
    case StudioModule::Hub:
        return "Hub";
    case StudioModule::Settings:
        return "Settings";
    }
    return "Chats";
}

inline StudioModule moduleFromTitle(std::string const &title) {
    if (title == "Models") {
        return StudioModule::Models;
    }
    if (title == "Hub") {
        return StudioModule::Hub;
    }
    if (title == "Settings") {
        return StudioModule::Settings;
    }
    return StudioModule::Chats;
}

inline int clampedChatIndex(AppState const &state) {
    if (state.chats.empty()) {
        return -1;
    }
    return std::clamp(state.selectedChatIndex, 0, static_cast<int>(state.chats.size() - 1));
}

inline std::string formatModelSize(std::size_t bytes) {
    if (bytes >= 1024ULL * 1024 * 1024) {
        return std::to_string(bytes / (1024ULL * 1024 * 1024)) + " GB";
    }
    if (bytes >= 1024ULL * 1024) {
        return std::to_string(bytes / (1024ULL * 1024)) + " MB";
    }
    if (bytes >= 1024ULL) {
        return std::to_string(bytes / 1024ULL) + " KB";
    }
    return std::to_string(bytes) + " B";
}

inline std::string modelDisplayName(std::string const &path) {
    std::size_t const slash = path.find_last_of('/');
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    if (name.size() > 5 && name.substr(name.size() - 5) == ".gguf") {
        name.resize(name.size() - 5);
    }
    return name;
}

inline std::string joinedTags(std::vector<std::string> const &tags, std::size_t maxCount = 3) {
    std::string result;
    std::size_t count = 0;
    for (std::string const &tag : tags) {
        if (tag.empty()) {
            continue;
        }
        if (count == maxCount) {
            break;
        }
        if (!result.empty()) {
            result += ", ";
        }
        result += tag;
        ++count;
    }
    return result;
}

inline char const *remoteModelSortLabel(RemoteModelSort sort) {
    switch (sort) {
    case RemoteModelSort::Downloads:
        return "Downloads";
    case RemoteModelSort::Likes:
        return "Likes";
    case RemoteModelSort::Updated:
        return "Updated";
    }
    return "Downloads";
}

inline char const *remoteModelVisibilityLabel(RemoteModelVisibilityFilter filter) {
    switch (filter) {
    case RemoteModelVisibilityFilter::All:
        return "All";
    case RemoteModelVisibilityFilter::PublicOnly:
        return "Public";
    case RemoteModelVisibilityFilter::GatedOnly:
        return "Gated";
    }
    return "All";
}

inline char const *downloadJobStatusLabel(DownloadJobStatus status) {
    switch (status) {
    case DownloadJobStatus::Running:
        return "Downloading";
    case DownloadJobStatus::Completed:
        return "Completed";
    case DownloadJobStatus::Failed:
        return "Failed";
    }
    return "Downloading";
}

inline float downloadJobProgress(DownloadJob const &job) {
    if (job.totalBytes == 0) {
        return 0.f;
    }
    float const progress = static_cast<float>(job.downloadedBytes) / static_cast<float>(job.totalBytes);
    return std::clamp(progress, 0.f, 1.f);
}

inline std::string remoteModelSearchCacheKey(
    std::string const &query,
    std::string const &author,
    RemoteModelSort sort,
    RemoteModelVisibilityFilter visibility
) {
    return "q=" + query + "\nauthor=" + author + "\nsort=" + remoteModelSortLabel(sort) +
           "\nvisibility=" + remoteModelVisibilityLabel(visibility);
}

inline AppState makeInitialAppState() {
    AppState state;
    if (!state.chats.empty()) {
        state.selectedChatIndex = 0;
    }
    return state;
}

} // namespace lambda
