#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "AppState.hpp"
#include "BackendTypes.hpp"

namespace lambda_backend {
struct LlmUiEvent;
}

namespace lambda {

struct PersistedChatState {
    std::vector<ChatThread> chats;
    std::string selectedChatId;
};

class IChatEngine {
  public:
    virtual ~IChatEngine() = default;

    virtual bool load(std::string const &modelPath, int nGpuLayers = -1, uint32_t nCtx = 0) = 0;
    virtual bool isLoaded() const = 0;
    virtual std::string const &modelPath() const = 0;
    virtual lambda_backend::SamplingParams samplingParams() const = 0;
    virtual void setSamplingParams(lambda_backend::SamplingParams const &params) = 0;
    virtual void unload() = 0;
    virtual void cancelGeneration() = 0;
    virtual void startChat(
        std::vector<lambda_backend::ChatMessage> messages,
        std::string chatId,
        std::uint64_t generationId,
        std::function<void(lambda_backend::LlmUiEvent)> post
    ) = 0;
};

class IModelManager {
  public:
    virtual ~IModelManager() = default;

    virtual std::uint64_t refreshLocalModels() = 0;
    virtual std::uint64_t searchHuggingFace(lambda_backend::HfSearchRequest request) = 0;
    virtual std::uint64_t listRepoFiles(std::string repoId) = 0;
    virtual std::uint64_t fetchRepoDetail(std::string repoId) = 0;
    virtual std::uint64_t inspectRepo(std::string repoId) = 0;
    virtual std::uint64_t downloadModel(std::string repoId, std::string fileName) = 0;
    virtual std::uint64_t loadModel(std::string path, int nGpuLayers = -1) = 0;
    virtual void unloadModel() = 0;
};

class IModelCatalogStore {
  public:
    virtual ~IModelCatalogStore() = default;

    virtual std::filesystem::path databasePath() const = 0;
    virtual void replaceSearchSnapshot(
        std::string const &query,
        std::vector<RemoteModel> const &models,
        std::string const &rawJson
    ) = 0;
    virtual std::vector<RemoteModel> loadSearchResults(std::string const &query) = 0;
    virtual std::vector<RemoteModel> searchCatalogModels(
        std::string const &query,
        std::string const &author,
        RemoteModelSort sort,
        RemoteModelVisibilityFilter visibility,
        std::size_t limit = 20
    ) = 0;
    virtual void replaceRepoFilesSnapshot(
        std::string const &repoId,
        std::vector<RemoteModelFile> const &files,
        std::string const &rawJson
    ) = 0;
    virtual std::vector<RemoteModelFile> loadRepoFiles(std::string const &repoId) = 0;
    virtual void replaceRepoDetailSnapshot(RemoteRepoDetail const &detail, std::string const &rawJson) = 0;
    virtual std::optional<RemoteRepoDetail> loadRepoDetail(std::string const &repoId) = 0;
    virtual void replaceLocalModelInstances(std::vector<LocalModel> const &models) = 0;
    virtual std::vector<LocalModel> loadLocalModelInstances() = 0;
    virtual void startDownloadJob(DownloadJob const &job) = 0;
    virtual void finishDownloadJob(
        std::string const &jobId,
        std::string const &localPath,
        std::int64_t finishedAtUnixMs
    ) = 0;
    virtual void failDownloadJob(
        std::string const &jobId,
        std::string const &errorText,
        std::int64_t finishedAtUnixMs
    ) = 0;
    virtual std::vector<DownloadJob> loadRecentDownloadJobs(std::size_t limit = 12) = 0;
    virtual void markRunningDownloadJobsInterrupted(std::int64_t finishedAtUnixMs) = 0;
    virtual PersistedChatState loadPersistedChatState() = 0;
    virtual void upsertChatThreadMeta(
        std::string const &chatId,
        std::string const &title,
        std::int64_t updatedAtUnixMs,
        std::string const &modelPath,
        std::string const &modelName,
        std::int64_t sortOrder
    ) = 0;
    virtual void replaceChatMessagesForThread(
        std::string const &chatId,
        std::vector<ChatMessage> const &messages
    ) = 0;
    virtual void updateSelectedChatId(std::string const &selectedChatId) = 0;
    virtual void replaceChatOrder(std::vector<std::string> const &chatIds) = 0;
};

} // namespace lambda
