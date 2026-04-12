#include <Flux.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "AppState.hpp"
#include "Backend.hpp"
#include "ChatsView.hpp"
#include "Divider.hpp"
#include "ModelsView.hpp"
#include "SettingsView.hpp"
#include "Sidebar.hpp"

using namespace flux;
using namespace lambda;

namespace {
std::once_flag gLambdaEventHandlers;

std::int64_t steadyNowNanos() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

std::int64_t systemNowMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void finishTrailingReasoningMessage(ChatThread &chat, std::int64_t finishedAtNanos) {
    if (chat.messages.empty()) {
        return;
    }
    lambda::ChatMessage &last = chat.messages.back();
    if (last.role == ChatRole::Reasoning && last.finishedAtNanos == 0) {
        last.finishedAtNanos = finishedAtNanos;
    }
}

void setChatModel(AppState &state, int chatIndex, std::string path, std::string name) {
    if (chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= state.chats.size()) {
        return;
    }
    ChatThread &chat = state.chats[static_cast<std::size_t>(chatIndex)];
    chat.modelPath = std::move(path);
    chat.modelName = std::move(name);
}

std::vector<lambda_backend::ChatMessage> toBackendMessages(ChatThread const &chat) {
    std::vector<lambda_backend::ChatMessage> result;
    result.reserve(chat.messages.size());
    for (lambda::ChatMessage const &message : chat.messages) {
        result.push_back(lambda_backend::ChatMessage {
            .role = toBackendRole(message.role),
            .text = message.text,
        });
    }
    return result;
}

std::string titleFromPrompt(std::string const &prompt) {
    std::string title = prompt;
    while (!title.empty() && std::isspace(static_cast<unsigned char>(title.front()))) {
        title.erase(title.begin());
    }
    if (title.empty()) {
        return "New chat";
    }
    if (title.size() > 40) {
        title.resize(40);
        title += "...";
    }
    return title;
}

} // namespace

struct LambdaStudio : ViewModifiers<LambdaStudio> {
    auto body() const {
        auto appState = useState<AppState>(makeInitialAppState());
        BackendServices &services = backend();
        auto currentRemoteSearchKey = [](AppState const &state) {
            return remoteModelSearchCacheKey(state.modelSearchQuery, state.modelSearchAuthor, state.remoteModelSort);
        };

        std::call_once(gLambdaEventHandlers, [appState, &services, currentRemoteSearchKey]() {
            Application::instance().eventQueue().on<lambda_backend::LlmUiEvent>(
                [appState](lambda_backend::LlmUiEvent const &event) {
                    AppState nextState = *appState;
                    auto it = std::find_if(nextState.chats.begin(), nextState.chats.end(), [&](ChatThread const &chat) {
                        return chat.id == event.chatId;
                    });
                    if (it == nextState.chats.end()) {
                        return;
                    }

                    std::int64_t const nowNanos = steadyNowNanos();

                    if (event.kind == lambda_backend::LlmUiEvent::Kind::Chunk) {
                        ChatRole const role = event.part == lambda_backend::LlmUiEvent::Part::Thinking
                                                  ? ChatRole::Reasoning
                                                  : ChatRole::Assistant;
                        if (role != ChatRole::Reasoning) {
                            finishTrailingReasoningMessage(*it, nowNanos);
                        }
                        if (it->messages.empty() || it->messages.back().role != role) {
                            it->messages.push_back(lambda::ChatMessage {
                                .role = role,
                                .text = "",
                                .startedAtNanos = role == ChatRole::Reasoning ? nowNanos : 0,
                                .collapsed = role == ChatRole::Reasoning,
                            });
                        }
                        it->messages.back().text += event.text;
                    } else if (event.kind == lambda_backend::LlmUiEvent::Kind::Done) {
                        finishTrailingReasoningMessage(*it, nowNanos);
                        it->streaming = false;
                        it->updatedAt = "now";
                        nextState.statusText = "Response complete";
                    } else if (event.kind == lambda_backend::LlmUiEvent::Kind::Error) {
                        finishTrailingReasoningMessage(*it, nowNanos);
                        it->streaming = false;
                        it->messages.push_back(lambda::ChatMessage {
                            .role = ChatRole::Assistant,
                            .text = std::string("[Error] ") + event.text,
                        });
                        it->updatedAt = "now";
                        nextState.errorText = event.text;
                    }

                    appState = std::move(nextState);
                }
            );

            Application::instance().eventQueue().on<lambda_backend::ModelManagerEvent>(
                [appState, &services, currentRemoteSearchKey](lambda_backend::ModelManagerEvent const &event) {
                    AppState nextState = *appState;
                    switch (event.kind) {
                    case lambda_backend::ModelManagerEvent::Kind::LocalModelsReady:
                        nextState.refreshingModels = false;
                        nextState.localModels.clear();
                        nextState.localModels.reserve(event.localModels.size());
                        for (lambda_backend::LocalModelInfo const &model : event.localModels) {
                            if (!model.path.empty()) {
                                nextState.localModels.push_back(toLocalModel(model));
                            }
                        }
                        try {
                            services.catalog->replaceLocalModelInstances(nextState.localModels);
                        } catch (std::exception const &e) {
                            nextState.errorText = e.what();
                        }
                        nextState.statusText = nextState.localModels.empty()
                                                   ? "No local models found"
                                                   : "Found " + std::to_string(nextState.localModels.size()) + " local model" +
                                                         (nextState.localModels.size() == 1 ? "" : "s");
                        break;
                    case lambda_backend::ModelManagerEvent::Kind::ModelLoaded:
                        nextState.modelLoading = false;
                        nextState.loadedModelPath = event.modelPath;
                        nextState.loadedModelName = event.modelName;
                        nextState.pendingModelPath.clear();
                        nextState.pendingModelName.clear();
                        nextState.statusText = "Loaded " + event.modelName;
                        nextState.errorText.clear();
                        break;
                    case lambda_backend::ModelManagerEvent::Kind::ModelLoadError:
                        nextState.modelLoading = false;
                        nextState.pendingModelPath.clear();
                        nextState.pendingModelName.clear();
                        nextState.errorText = event.error;
                        break;
                    case lambda_backend::ModelManagerEvent::Kind::DownloadDone:
                        nextState.downloadingModel = false;
                        if (!nextState.pendingDownloadJobId.empty()) {
                            for (DownloadJob &job : nextState.recentDownloadJobs) {
                                if (job.id != nextState.pendingDownloadJobId) {
                                    continue;
                                }
                                job.status = DownloadJobStatus::Completed;
                                job.localPath = event.modelPath;
                                job.error.clear();
                                job.finishedAtUnixMs = systemNowMillis();
                                try {
                                    services.catalog->finishDownloadJob(job.id, job.localPath, job.finishedAtUnixMs);
                                } catch (std::exception const &e) {
                                    nextState.errorText = e.what();
                                }
                                break;
                            }
                        }
                        nextState.pendingDownloadJobId.clear();
                        nextState.pendingDownloadRepoId.clear();
                        nextState.pendingDownloadFilePath.clear();
                        nextState.statusText = "Downloaded " + event.modelName;
                        nextState.errorText.clear();
                        break;
                    case lambda_backend::ModelManagerEvent::Kind::DownloadError:
                        nextState.downloadingModel = false;
                        if (!nextState.pendingDownloadJobId.empty()) {
                            for (DownloadJob &job : nextState.recentDownloadJobs) {
                                if (job.id != nextState.pendingDownloadJobId) {
                                    continue;
                                }
                                job.status = DownloadJobStatus::Failed;
                                job.error = event.error;
                                job.finishedAtUnixMs = systemNowMillis();
                                try {
                                    services.catalog->failDownloadJob(job.id, job.error, job.finishedAtUnixMs);
                                } catch (std::exception const &e) {
                                    nextState.errorText = e.what();
                                }
                                break;
                            }
                        }
                        nextState.pendingDownloadJobId.clear();
                        nextState.pendingDownloadRepoId.clear();
                        nextState.pendingDownloadFilePath.clear();
                        nextState.errorText = event.error;
                        break;
                    case lambda_backend::ModelManagerEvent::Kind::HfSearchReady:
                        nextState.searchingRemoteModels = false;
                        if (event.searchKey != currentRemoteSearchKey(nextState)) {
                            if (event.error.empty()) {
                                std::vector<RemoteModel> cachedModels;
                                cachedModels.reserve(event.hfModels.size());
                                for (lambda_backend::HfModelInfo const &model : event.hfModels) {
                                    cachedModels.push_back(toRemoteModel(model));
                                }
                                try {
                                    services.catalog->replaceSearchSnapshot(
                                        event.searchKey,
                                        cachedModels,
                                        event.rawJson
                                    );
                                } catch (...) {
                                }
                            }
                            break;
                        }
                        if (!event.error.empty()) {
                            nextState.errorText = event.error;
                            if (nextState.remoteModels.empty()) {
                                nextState.selectedRemoteRepoId.clear();
                                nextState.selectedRemoteRepoFiles.clear();
                                nextState.selectedRemoteRepoDetail.reset();
                                nextState.loadingRemoteModelFiles = false;
                                nextState.loadingRemoteRepoDetail = false;
                            } else {
                                nextState.statusText = "Search refresh failed, showing cached results";
                            }
                        } else {
                            nextState.errorText.clear();
                            nextState.remoteModels.clear();
                            nextState.remoteModels.reserve(event.hfModels.size());
                            for (lambda_backend::HfModelInfo const &model : event.hfModels) {
                                nextState.remoteModels.push_back(toRemoteModel(model));
                            }
                            try {
                                services.catalog->replaceSearchSnapshot(
                                    event.searchKey,
                                    nextState.remoteModels,
                                    event.rawJson
                                );
                            } catch (std::exception const &e) {
                                nextState.errorText = e.what();
                            }

                            if (nextState.remoteModels.empty()) {
                                nextState.selectedRemoteRepoId.clear();
                                nextState.selectedRemoteRepoFiles.clear();
                                nextState.selectedRemoteRepoDetail.reset();
                                nextState.loadingRemoteModelFiles = false;
                                nextState.loadingRemoteRepoDetail = false;
                                nextState.statusText = "No matching Hugging Face models";
                            } else {
                                bool foundSelection = false;
                                for (RemoteModel const &model : nextState.remoteModels) {
                                    if (model.id == nextState.selectedRemoteRepoId) {
                                        foundSelection = true;
                                        break;
                                    }
                                }
                                if (!foundSelection) {
                                    nextState.selectedRemoteRepoId = nextState.remoteModels.front().id;
                                    nextState.selectedRemoteRepoFiles.clear();
                                    nextState.selectedRemoteRepoDetail.reset();
                                    nextState.loadingRemoteModelFiles = true;
                                    nextState.loadingRemoteRepoDetail = true;
                                    services.manager->inspectRepo(nextState.selectedRemoteRepoId);
                                }
                                nextState.statusText =
                                    "Found " + std::to_string(nextState.remoteModels.size()) + " matching model" +
                                    (nextState.remoteModels.size() == 1 ? "" : "s");
                            }
                        }
                        break;
                    case lambda_backend::ModelManagerEvent::Kind::HfFilesReady:
                        if (event.repoId == nextState.selectedRemoteRepoId) {
                            nextState.loadingRemoteModelFiles = false;
                        }
                        if (!event.error.empty()) {
                            nextState.errorText = event.error;
                            if (event.repoId == nextState.selectedRemoteRepoId && !nextState.selectedRemoteRepoFiles.empty()) {
                                nextState.statusText = "File refresh failed, showing cached repo files";
                            }
                        } else {
                            std::vector<RemoteModelFile> files;
                            files.reserve(event.hfFiles.size());
                            for (lambda_backend::HfFileInfo const &file : event.hfFiles) {
                                files.push_back(toRemoteModelFile(file));
                            }
                            try {
                                services.catalog->replaceRepoFilesSnapshot(
                                    event.repoId,
                                    files,
                                    event.rawJson
                                );
                            } catch (std::exception const &e) {
                                nextState.errorText = e.what();
                            }
                            if (event.repoId == nextState.selectedRemoteRepoId) {
                                nextState.errorText.clear();
                                nextState.selectedRemoteRepoFiles = std::move(files);
                                nextState.statusText = event.hfFiles.empty()
                                                           ? "No GGUF files in selected repo"
                                                           : "Found " + std::to_string(event.hfFiles.size()) + " GGUF file" +
                                                                 (event.hfFiles.size() == 1 ? "" : "s");
                            }
                        }
                        break;
                    case lambda_backend::ModelManagerEvent::Kind::HfRepoDetailReady:
                        if (event.repoId == nextState.selectedRemoteRepoId) {
                            nextState.loadingRemoteRepoDetail = false;
                        }
                        if (!event.error.empty()) {
                            nextState.errorText = event.error;
                            if (event.repoId == nextState.selectedRemoteRepoId &&
                                nextState.selectedRemoteRepoDetail.has_value()) {
                                nextState.statusText = "Repo detail refresh failed, showing cached metadata";
                            }
                        } else {
                            RemoteRepoDetail detail = toRemoteRepoDetail(event.hfRepoDetail);
                            try {
                                services.catalog->replaceRepoDetailSnapshot(detail, event.rawJson);
                            } catch (std::exception const &e) {
                                nextState.errorText = e.what();
                            }
                            if (event.repoId == nextState.selectedRemoteRepoId) {
                                nextState.errorText.clear();
                                nextState.selectedRemoteRepoDetail = std::move(detail);
                            }
                        }
                        break;
                    }
                    appState = std::move(nextState);
                }
            );

            AppState nextState = *appState;
            nextState.refreshingModels = true;
            try {
                services.catalog->markRunningDownloadJobsInterrupted(systemNowMillis());
                nextState.recentDownloadJobs = services.catalog->loadRecentDownloadJobs();
                nextState.localModels = services.catalog->loadLocalModelInstances();
            } catch (std::exception const &e) {
                nextState.errorText = e.what();
            }
            nextState.loadedModelPath = lambda_backend::defaultModelPath();
            nextState.loadedModelName = lambda_backend::defaultModelName();
            if (!nextState.loadedModelPath.empty() && !services.engine->isLoaded()) {
                nextState.modelLoading = true;
                nextState.pendingModelPath = nextState.loadedModelPath;
                nextState.pendingModelName = nextState.loadedModelName;
            }
            appState = std::move(nextState);

            services.manager->refreshLocalModels();
            if (!lambda_backend::defaultModelPath().empty() && !services.engine->isLoaded()) {
                services.manager->loadModel(
                    lambda_backend::defaultModelPath(),
                    lambda_backend::defaultNGpuLayers()
                );
            }
        });

        AppState state = *appState;

        auto requestInventoryRefresh = [appState, &services]() {
            AppState nextState = *appState;
            nextState.refreshingModels = true;
            nextState.statusText = "Refreshing model inventory...";
            nextState.errorText.clear();
            appState = std::move(nextState);
            services.manager->refreshLocalModels();
        };

        auto updateModelSearchQuery = [appState](std::string const &query) {
            AppState nextState = *appState;
            nextState.modelSearchQuery = query;
            appState = std::move(nextState);
        };

        auto updateModelSearchAuthor = [appState](std::string const &author) {
            AppState nextState = *appState;
            nextState.modelSearchAuthor = author;
            appState = std::move(nextState);
        };

        auto updateRemoteModelSort = [appState](RemoteModelSort sort) {
            AppState nextState = *appState;
            nextState.remoteModelSort = sort;
            appState = std::move(nextState);
        };

        auto requestRemoteSearch = [appState, &services, currentRemoteSearchKey](
                                       std::string query,
                                       std::string author,
                                       RemoteModelSort sort
                                   ) {
            AppState nextState = *appState;
            nextState.modelSearchQuery = query;
            nextState.modelSearchAuthor = author;
            nextState.remoteModelSort = sort;
            nextState.searchingRemoteModels = true;
            std::string const cacheKey = currentRemoteSearchKey(nextState);
            try {
                nextState.remoteModels = services.catalog->loadSearchResults(cacheKey);
                if (nextState.remoteModels.empty()) {
                    nextState.remoteModels = services.catalog->searchCatalogModels(query, author, sort);
                }
                nextState.selectedRemoteRepoId.clear();
                nextState.selectedRemoteRepoFiles.clear();
                nextState.selectedRemoteRepoDetail.reset();
                nextState.loadingRemoteModelFiles = false;
                nextState.loadingRemoteRepoDetail = false;
                if (!nextState.remoteModels.empty()) {
                    nextState.selectedRemoteRepoId = nextState.remoteModels.front().id;
                    nextState.selectedRemoteRepoFiles = services.catalog->loadRepoFiles(nextState.selectedRemoteRepoId);
                    nextState.selectedRemoteRepoDetail = services.catalog->loadRepoDetail(nextState.selectedRemoteRepoId);
                    nextState.loadingRemoteModelFiles = nextState.selectedRemoteRepoFiles.empty();
                    nextState.loadingRemoteRepoDetail = !nextState.selectedRemoteRepoDetail.has_value();
                }
            } catch (std::exception const &e) {
                nextState.remoteModels.clear();
                nextState.selectedRemoteRepoId.clear();
                nextState.selectedRemoteRepoFiles.clear();
                nextState.selectedRemoteRepoDetail.reset();
                nextState.loadingRemoteModelFiles = false;
                nextState.loadingRemoteRepoDetail = false;
                nextState.errorText = e.what();
            }
            nextState.statusText = query.empty() && author.empty() ? "Searching top GGUF models..." :
                                                                  "Searching Hugging Face...";
            appState = std::move(nextState);
            services.manager->searchHuggingFace(lambda_backend::HfSearchRequest {
                .query = std::move(query),
                .author = std::move(author),
                .sortKey = sort == RemoteModelSort::Likes ? "likes" :
                           sort == RemoteModelSort::Updated ? "lastModified" :
                                                              "downloads",
                .cacheKey = cacheKey,
            });
        };

        auto requestRemoteRepoFiles = [appState, &services](std::string repoId) {
            if (repoId.empty()) {
                return;
            }
            AppState nextState = *appState;
            nextState.selectedRemoteRepoId = repoId;
            try {
                nextState.selectedRemoteRepoFiles = services.catalog->loadRepoFiles(repoId);
                nextState.selectedRemoteRepoDetail = services.catalog->loadRepoDetail(repoId);
                nextState.loadingRemoteModelFiles = nextState.selectedRemoteRepoFiles.empty();
                nextState.loadingRemoteRepoDetail = !nextState.selectedRemoteRepoDetail.has_value();
                nextState.errorText.clear();
            } catch (std::exception const &e) {
                nextState.selectedRemoteRepoFiles.clear();
                nextState.selectedRemoteRepoDetail.reset();
                nextState.loadingRemoteModelFiles = true;
                nextState.loadingRemoteRepoDetail = true;
                nextState.errorText = e.what();
            }
            nextState.statusText = "Loading GGUF files...";
            appState = std::move(nextState);
            services.manager->inspectRepo(std::move(repoId));
        };

        auto requestRemoteDownload = [appState, &services](std::string repoId, std::string path) {
            if (repoId.empty() || path.empty()) {
                return;
            }
            AppState nextState = *appState;
            DownloadJob job;
            job.id = repoId + "|" + path + "|" + std::to_string(systemNowMillis());
            job.repoId = repoId;
            job.filePath = path;
            job.startedAtUnixMs = systemNowMillis();
            job.status = DownloadJobStatus::Running;
            nextState.recentDownloadJobs.insert(nextState.recentDownloadJobs.begin(), job);
            if (nextState.recentDownloadJobs.size() > 12) {
                nextState.recentDownloadJobs.resize(12);
            }
            nextState.downloadingModel = true;
            nextState.pendingDownloadJobId = job.id;
            nextState.pendingDownloadRepoId = repoId;
            nextState.pendingDownloadFilePath = path;
            nextState.statusText = "Downloading " + path;
            nextState.errorText.clear();
            try {
                services.catalog->startDownloadJob(job);
            } catch (std::exception const &e) {
                nextState.errorText = e.what();
            }
            appState = std::move(nextState);
            services.manager->downloadModel(std::move(repoId), std::move(path));
        };

        auto retryDownloadJob = [requestRemoteDownload](std::string repoId, std::string filePath) {
            requestRemoteDownload(std::move(repoId), std::move(filePath));
        };

        auto requestModelLoad = [appState, &services](std::string const &path, std::string const &name) {
            if (path.empty()) {
                return;
            }
            AppState nextState = *appState;
            nextState.modelLoading = true;
            nextState.pendingModelPath = path;
            nextState.pendingModelName = name;
            nextState.statusText = "Loading " + name;
            nextState.errorText.clear();
            appState = std::move(nextState);
            services.manager->loadModel(path, lambda_backend::defaultNGpuLayers());
        };

        auto selectChatModel = [appState, requestModelLoad](int chatIndex, std::string const &path, std::string const &name) {
            AppState nextState = *appState;
            setChatModel(nextState, chatIndex, path, name);
            appState = nextState;
            requestModelLoad(path, name);
        };

        auto sendMessage = [appState, &services](int chatIndex, std::string const &message) {
            if (message.empty()) {
                return;
            }
            AppState nextState = *appState;
            if (chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= nextState.chats.size()) {
                return;
            }

            ChatThread &chat = nextState.chats[static_cast<std::size_t>(chatIndex)];
            if (chat.streaming || chat.modelPath.empty() || chat.modelPath != nextState.loadedModelPath || nextState.modelLoading) {
                return;
            }

            if (chat.messages.empty() || chat.title == "New chat") {
                chat.title = titleFromPrompt(message);
            }

            chat.messages.push_back(lambda::ChatMessage {
                .role = ChatRole::User,
                .text = message,
            });
            chat.streaming = true;
            chat.updatedAt = "now";
            nextState.errorText.clear();
            nextState.statusText = "Generating response...";

            std::vector<lambda_backend::ChatMessage> history = toBackendMessages(chat);
            std::string streamChatId = chat.id;

            appState = std::move(nextState);

            services.engine->startChat(
                std::move(history),
                std::move(streamChatId),
                [](lambda_backend::LlmUiEvent event) {
                    Application::instance().eventQueue().post(std::move(event));
                }
            );
        };

        auto stopMessage = [appState, &services](int chatIndex) {
            services.engine->cancelGeneration();
            AppState nextState = *appState;
            if (chatIndex >= 0 && static_cast<std::size_t>(chatIndex) < nextState.chats.size()) {
                nextState.chats[static_cast<std::size_t>(chatIndex)].streaming = false;
            }
            nextState.statusText = "Generation stopped";
            appState = std::move(nextState);
        };

        auto toggleReasoningMessage = [appState](int chatIndex, int messageIndex) {
            AppState nextState = *appState;
            if (chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= nextState.chats.size()) {
                return;
            }
            ChatThread &chat = nextState.chats[static_cast<std::size_t>(chatIndex)];
            if (messageIndex < 0 || static_cast<std::size_t>(messageIndex) >= chat.messages.size()) {
                return;
            }
            lambda::ChatMessage &message = chat.messages[static_cast<std::size_t>(messageIndex)];
            if (message.role != ChatRole::Reasoning) {
                return;
            }
            message.collapsed = !message.collapsed;
            appState = std::move(nextState);
        };

        auto createChat = [appState]() {
            AppState nextState = *appState;
            ChatThread thread;
            thread.id = lambda::generateChatId();
            thread.title = "New chat";
            thread.updatedAt = "now";
            thread.modelPath = nextState.loadedModelPath;
            thread.modelName = nextState.loadedModelName;
            nextState.chats.push_back(std::move(thread));
            nextState.selectedChatIndex = static_cast<int>(nextState.chats.size() - 1);
            appState = std::move(nextState);
        };

        Element currentView = ChatsView {
            .state = state,
            .onNewChat = createChat,
            .onSelectChat = [appState](int index) {
                AppState nextState = *appState;
                nextState.selectedChatIndex = index;
                appState = std::move(nextState);
            },
            .onSelectModel = selectChatModel,
            .onSend = sendMessage,
            .onStop = stopMessage,
            .onToggleReasoning = toggleReasoningMessage,
        }
                                  .flex(1.f, 1.f);

        if (state.currentModule == StudioModule::Models) {
            currentView = ModelsView {
                .state = state,
                .onRefresh = requestInventoryRefresh,
                .onLoad = requestModelLoad,
                .onSearchQueryChange = updateModelSearchQuery,
                .onSearchAuthorChange = updateModelSearchAuthor,
                .onSortChange = updateRemoteModelSort,
                .onSearch = requestRemoteSearch,
                .onSelectRemoteRepo = requestRemoteRepoFiles,
                .onDownload = requestRemoteDownload,
                .onRetryDownload = retryDownloadJob,
            }
                              .flex(1.f, 1.f);
        } else if (state.currentModule == StudioModule::Settings) {
            currentView = SettingsView {
                .state = state,
            }
                              .flex(1.f, 1.f);
        }

        return HStack {
            .spacing = 0.f,
            .alignment = Alignment::Stretch,
            .children = children(
                Sidebar {
                    .modules = {
                        {IconName::ChatBubble, "Chats"},
                        {IconName::ModelTraining, "Models"},
                        {IconName::Settings, "Settings"}
                    },
                    .selectedTitle = moduleTitle(state.currentModule),
                    .onSelect = [appState](std::string title) {
                        AppState nextState = *appState;
                        nextState.currentModule = moduleFromTitle(title);
                        appState = std::move(nextState);
                    },
                }
                    .flex(0.f, 0.f),
                Divider {.orientation = Divider::Orientation::Vertical}, std::move(currentView)
            ),
        };
    }
};

int main(int argc, char *argv[]) {
    Application app(argc, argv);

    auto &w = app.createWindow<Window>({
        .size = {1100, 720},
        .title = "Lambda Studio",
        .resizable = true,
    });

    w.setView<LambdaStudio>();

    int const code = app.exec();
    shutdownBackend();
    return code;
}
