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
#include "HubView.hpp"
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

std::string selectedChatIdForState(AppState const &state) {
    int const selectedIndex = clampedChatIndex(state);
    if (selectedIndex < 0) {
        return {};
    }
    return state.chats[static_cast<std::size_t>(selectedIndex)].id;
}

void restoreSelectedChat(AppState &state, std::string const &selectedChatId) {
    if (state.chats.empty()) {
        state.selectedChatIndex = 0;
        return;
    }

    auto const it = std::find_if(state.chats.begin(), state.chats.end(), [&](ChatThread const &chat) {
        return chat.id == selectedChatId;
    });
    if (it != state.chats.end()) {
        state.selectedChatIndex = static_cast<int>(std::distance(state.chats.begin(), it));
        return;
    }
    state.selectedChatIndex = 0;
}

void persistChats(AppState &state, BackendServices &services) {
    try {
        services.catalog->replaceChats(state.chats, selectedChatIdForState(state));
    } catch (std::exception const &e) {
        state.errorText = e.what();
    }
}

MessageGenerationStats toMessageGenerationStats(
    lambda_backend::GenerationStats const &stats,
    ChatThread const &chat
) {
    return MessageGenerationStats {
        .modelPath = chat.modelPath,
        .modelName = chat.modelName,
        .promptTokens = stats.promptTokens,
        .completionTokens = stats.completionTokens,
        .startedAtUnixMs = stats.startedAtUnixMs,
        .firstTokenAtUnixMs = stats.firstTokenAtUnixMs,
        .finishedAtUnixMs = stats.finishedAtUnixMs,
        .tokensPerSecond = stats.tokensPerSecond,
        .status = stats.status,
        .errorText = stats.errorText,
        .temp = stats.temp,
        .topP = stats.topP,
        .topK = stats.topK,
        .maxTokens = stats.maxTokens,
    };
}

void attachGenerationStatsToLatestResponse(
    ChatThread &chat,
    lambda_backend::GenerationStats const &stats
) {
    MessageGenerationStats const messageStats = toMessageGenerationStats(stats, chat);

    for (auto it = chat.messages.rbegin(); it != chat.messages.rend(); ++it) {
        if (it->role == ChatRole::Assistant) {
            it->generationStats = messageStats;
            return;
        }
    }

    for (auto it = chat.messages.rbegin(); it != chat.messages.rend(); ++it) {
        if (it->role == ChatRole::Reasoning) {
            it->generationStats = messageStats;
            return;
        }
    }
}

void finishTrailingReasoningMessage(std::vector<ChatMessage> &messages, std::int64_t finishedAtNanos) {
    if (messages.empty()) {
        return;
    }
    lambda::ChatMessage &last = messages.back();
    if (last.role == ChatRole::Reasoning && last.finishedAtNanos == 0) {
        last.finishedAtNanos = finishedAtNanos;
    }
}

void finishTrailingReasoningMessage(ChatThread &chat, std::int64_t finishedAtNanos) {
    finishTrailingReasoningMessage(chat.messages, finishedAtNanos);
}

void commitStreamDraft(ChatThread &chat, std::int64_t finishedAtNanos) {
    finishTrailingReasoningMessage(chat.streamDraftMessages, finishedAtNanos);
    for (ChatMessage &message : chat.streamDraftMessages) {
        if (message.text.empty()) {
            continue;
        }
        syncAssistantParagraphs(message);
        chat.messages.push_back(std::move(message));
    }
    chat.streamDraftMessages.clear();
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

struct NoticeBanner : ViewModifiers<NoticeBanner> {
    AppNotice notice;
    std::function<void()> onOpen;
    std::function<void()> onDismiss;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        return HStack {
            .spacing = theme.space3,
            .alignment = Alignment::Center,
            .children = children(
                Icon {
                    .name = IconName::CloudDownload,
                    .size = 20.f,
                    .weight = 600.f,
                    .color = theme.colorAccent,
                },
                VStack {
                    .spacing = 2.f,
                    .alignment = Alignment::Start,
                    .children = children(
                        Text {
                            .text = notice.title,
                            .font = theme.fontLabel,
                            .color = theme.colorTextPrimary,
                            .horizontalAlignment = HorizontalAlignment::Leading,
                        },
                        Text {
                            .text = notice.detail,
                            .font = theme.fontBodySmall,
                            .color = theme.colorTextSecondary,
                            .horizontalAlignment = HorizontalAlignment::Leading,
                            .wrapping = TextWrapping::Wrap,
                        }
                    )
                }
                    .flex(1.f, 1.f),
                LinkButton {
                    .label = "Open Models",
                    .onTap = onOpen,
                },
                IconButton {
                    .icon = IconName::Close,
                    .style = {
                        .size = 18.f,
                        .weight = 500.f,
                        .color = theme.colorTextSecondary,
                    },
                    .onTap = onDismiss,
                }
            )
        }
            .padding(theme.space3)
            .fill(FillStyle::solid(theme.colorSurfaceOverlay))
            .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
            .cornerRadius(theme.radiusLarge);
    }
};

struct LambdaStudio : ViewModifiers<LambdaStudio> {
    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        auto appState = useState<AppState>(makeInitialAppState());
        BackendServices &services = backend();
        auto currentRemoteSearchKey = [](AppState const &state) {
            return remoteModelSearchCacheKey(
                state.modelSearchQuery,
                state.modelSearchAuthor,
                state.remoteModelSort,
                state.remoteModelVisibility
            );
        };

        std::call_once(gLambdaEventHandlers, [appState, &services, currentRemoteSearchKey]() {
            Application::instance().eventQueue().on<lambda_backend::LlmUiEvent>(
                [appState, &services](lambda_backend::LlmUiEvent const &event) {
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
                            finishTrailingReasoningMessage(it->streamDraftMessages, nowNanos);
                        }
                        if (it->streamDraftMessages.empty() || it->streamDraftMessages.back().role != role) {
                            it->streamDraftMessages.push_back(lambda::ChatMessage {
                                .role = role,
                                .text = "",
                                .startedAtNanos = role == ChatRole::Reasoning ? nowNanos : 0,
                                .collapsed = role == ChatRole::Reasoning,
                            });
                        }
                        appendChatMessageText(it->streamDraftMessages.back(), event.text);
                    } else if (event.kind == lambda_backend::LlmUiEvent::Kind::Done) {
                        commitStreamDraft(*it, nowNanos);
                        it->streaming = false;
                        it->updatedAtUnixMs = currentUnixMillis();
                        if (event.generationStats.has_value()) {
                            attachGenerationStatsToLatestResponse(*it, *event.generationStats);
                        }
                        nextState.statusText = "Response complete";
                    } else if (event.kind == lambda_backend::LlmUiEvent::Kind::Error) {
                        commitStreamDraft(*it, nowNanos);
                        it->streaming = false;
                        it->messages.push_back(lambda::ChatMessage {
                            .role = ChatRole::Assistant,
                            .text = std::string("[Error] ") + event.text,
                        });
                        if (event.generationStats.has_value()) {
                            it->messages.back().generationStats = toMessageGenerationStats(*event.generationStats, *it);
                        }
                        syncAssistantParagraphs(it->messages.back());
                        it->updatedAtUnixMs = currentUnixMillis();
                        nextState.errorText = event.text;
                    }

                    persistChats(nextState, services);
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
                        nextState.notice.reset();
                        nextState.statusText = "Loaded " + event.modelName;
                        nextState.errorText.clear();
                        break;
                    case lambda_backend::ModelManagerEvent::Kind::ModelLoadError:
                        nextState.modelLoading = false;
                        nextState.pendingModelPath.clear();
                        nextState.pendingModelName.clear();
                        nextState.notice.reset();
                        nextState.errorText = event.error;
                        break;
                    case lambda_backend::ModelManagerEvent::Kind::DownloadProgress:
                        if (!nextState.pendingDownloadJobId.empty()) {
                            for (DownloadJob &job : nextState.recentDownloadJobs) {
                                if (job.id != nextState.pendingDownloadJobId) {
                                    continue;
                                }
                                job.downloadedBytes = event.downloadedBytes;
                                job.totalBytes = event.totalBytes;
                                break;
                            }
                        }
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
                                job.finishedAtUnixMs = currentUnixMillis();
                                job.downloadedBytes = event.totalBytes;
                                job.totalBytes = event.totalBytes;
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
                        nextState.notice.reset();
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
                                job.finishedAtUnixMs = currentUnixMillis();
                                if (event.downloadedBytes > 0) {
                                    job.downloadedBytes = event.downloadedBytes;
                                }
                                if (event.totalBytes > 0) {
                                    job.totalBytes = event.totalBytes;
                                }
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
                        nextState.notice.reset();
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
                PersistedChatState persistedChatState = services.catalog->loadPersistedChatState();
                nextState.chats = std::move(persistedChatState.chats);
                restoreSelectedChat(nextState, persistedChatState.selectedChatId);
                services.catalog->markRunningDownloadJobsInterrupted(currentUnixMillis());
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

        AppState const &state = *appState;

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

        auto updateRemoteModelVisibility = [appState](RemoteModelVisibilityFilter visibility) {
            AppState nextState = *appState;
            nextState.remoteModelVisibility = visibility;
            appState = std::move(nextState);
        };

        auto requestRemoteSearch = [appState, &services, currentRemoteSearchKey](
                                       std::string query,
                                       std::string author,
                                       RemoteModelSort sort,
                                       RemoteModelVisibilityFilter visibility
                                   ) {
            AppState nextState = *appState;
            nextState.modelSearchQuery = query;
            nextState.modelSearchAuthor = author;
            nextState.remoteModelSort = sort;
            nextState.remoteModelVisibility = visibility;
            nextState.searchingRemoteModels = true;
            std::string const cacheKey = currentRemoteSearchKey(nextState);
            try {
                nextState.remoteModels = services.catalog->loadSearchResults(cacheKey);
                if (nextState.remoteModels.empty()) {
                    nextState.remoteModels = services.catalog->searchCatalogModels(query, author, sort, visibility);
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
                .visibilityFilter = visibility == RemoteModelVisibilityFilter::PublicOnly ? "public" :
                                    visibility == RemoteModelVisibilityFilter::GatedOnly ? "gated" :
                                                                                           "all",
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
            job.id = repoId + "|" + path + "|" + std::to_string(currentUnixMillis());
            job.repoId = repoId;
            job.filePath = path;
            job.startedAtUnixMs = currentUnixMillis();
            job.status = DownloadJobStatus::Running;
            job.downloadedBytes = 0;
            job.totalBytes = 0;
            nextState.recentDownloadJobs.insert(nextState.recentDownloadJobs.begin(), job);
            if (nextState.recentDownloadJobs.size() > 12) {
                nextState.recentDownloadJobs.resize(12);
            }
            nextState.downloadingModel = true;
            nextState.pendingDownloadJobId = job.id;
            nextState.pendingDownloadRepoId = repoId;
            nextState.pendingDownloadFilePath = path;
            nextState.notice = AppNotice {
                .title = "Download started",
                .detail = path + " is downloading. Follow progress in the Models view.",
                .targetModule = StudioModule::Models,
            };
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

        auto dismissNotice = [appState]() {
            AppState nextState = *appState;
            nextState.notice.reset();
            appState = std::move(nextState);
        };

        auto openNoticeTarget = [appState]() {
            AppState nextState = *appState;
            if (nextState.notice.has_value()) {
                nextState.currentModule = nextState.notice->targetModule;
            }
            nextState.notice.reset();
            appState = std::move(nextState);
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

        auto selectChatModel = [appState, &services, requestModelLoad](int chatIndex, std::string const &path, std::string const &name) {
            AppState nextState = *appState;
            setChatModel(nextState, chatIndex, path, name);
            persistChats(nextState, services);
            appState = nextState;
            requestModelLoad(path, name);
        };

        auto sendMessage = [appState, &services](int chatIndex, std::string const &message) {
            if (message.empty()) {
                return;
            }
            bool const fakeStreaming = lambda_backend::debugFakeStreamEnabled();
            AppState nextState = *appState;
            if (chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= nextState.chats.size()) {
                return;
            }

            ChatThread &chat = nextState.chats[static_cast<std::size_t>(chatIndex)];
            if (
                chat.streaming ||
                (!fakeStreaming &&
                 (chat.modelPath.empty() || chat.modelPath != nextState.loadedModelPath || nextState.modelLoading))
            ) {
                return;
            }

            if (fakeStreaming && chat.modelPath.empty()) {
                chat.modelPath = "__debug_fake_stream__";
                chat.modelName = "Debug Stream";
                nextState.loadedModelPath = chat.modelPath;
                nextState.loadedModelName = chat.modelName;
            }

            if (chat.messages.empty() || chat.title == "New chat") {
                chat.title = titleFromPrompt(message);
            }

            chat.streamDraftMessages.clear();
            chat.updatedAtUnixMs = currentUnixMillis();
            chat.messages.push_back(lambda::ChatMessage {
                .role = ChatRole::User,
                .text = message,
            });
            chat.streaming = true;
            nextState.errorText.clear();
            nextState.statusText = "Generating response...";

            std::vector<lambda_backend::ChatMessage> history = toBackendMessages(chat);
            std::string streamChatId = chat.id;

            persistChats(nextState, services);
            appState = std::move(nextState);

            services.engine->startChat(
                std::move(history),
                std::move(streamChatId),
                [](lambda_backend::LlmUiEvent event) {
                    Application::instance().eventQueue().post(std::move(event));
                }
            );
        };

        auto didAutoStream = useState(false);
        if (lambda_backend::debugAutoStreamEnabled() && !(*didAutoStream) && !state.chats.empty()) {
            didAutoStream = true;
            Application::instance().onNextFrameNeeded([sendMessage] {
                sendMessage(0, "Run the markdown stress stream.");
            });
        }

        auto stopMessage = [appState, &services](int chatIndex) {
            services.engine->cancelGeneration();
            AppState nextState = *appState;
            if (chatIndex >= 0 && static_cast<std::size_t>(chatIndex) < nextState.chats.size()) {
                nextState.chats[static_cast<std::size_t>(chatIndex)].streaming = false;
            }
            nextState.statusText = "Generation stopped";
            persistChats(nextState, services);
            appState = std::move(nextState);
        };

        auto toggleReasoningMessage = [appState, &services](int chatIndex, int messageIndex) {
            AppState nextState = *appState;
            if (chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= nextState.chats.size()) {
                return;
            }
            ChatThread &chat = nextState.chats[static_cast<std::size_t>(chatIndex)];
            if (messageIndex < 0) {
                return;
            }
            std::size_t const index = static_cast<std::size_t>(messageIndex);
            lambda::ChatMessage *message = nullptr;
            if (index < chat.messages.size()) {
                message = &chat.messages[index];
            } else {
                std::size_t const draftIndex = index - chat.messages.size();
                if (draftIndex < chat.streamDraftMessages.size()) {
                    message = &chat.streamDraftMessages[draftIndex];
                }
            }
            if (message == nullptr) {
                return;
            }
            if (message->role != ChatRole::Reasoning) {
                return;
            }
            message->collapsed = !message->collapsed;
            persistChats(nextState, services);
            appState = std::move(nextState);
        };

        auto createChat = [appState, &services]() {
            AppState nextState = *appState;
            ChatThread thread;
            thread.id = lambda::generateChatId();
            thread.title = "New chat";
            thread.updatedAtUnixMs = currentUnixMillis();
            thread.modelPath = nextState.loadedModelPath;
            thread.modelName = nextState.loadedModelName;
            nextState.chats.push_back(std::move(thread));
            nextState.selectedChatIndex = static_cast<int>(nextState.chats.size() - 1);
            persistChats(nextState, services);
            appState = std::move(nextState);
        };

        Element currentView = ChatsView {
            .state = state,
            .onNewChat = createChat,
            .onSelectChat = [appState, &services](int index) {
                AppState nextState = *appState;
                nextState.selectedChatIndex = index;
                persistChats(nextState, services);
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
                .onRetryDownload = retryDownloadJob,
            }
                              .flex(1.f, 1.f);
        } else if (state.currentModule == StudioModule::Hub) {
            currentView = HubView {
                .state = state,
                .onSearchQueryChange = updateModelSearchQuery,
                .onSearchAuthorChange = updateModelSearchAuthor,
                .onSortChange = updateRemoteModelSort,
                .onVisibilityChange = updateRemoteModelVisibility,
                .onSearch = requestRemoteSearch,
                .onSelectRemoteRepo = requestRemoteRepoFiles,
                .onDownload = requestRemoteDownload,
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
                        {IconName::Cloud, "Hub"},
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
                Divider {.orientation = Divider::Orientation::Vertical},
                VStack {
                    .spacing = theme.space3,
                    .alignment = Alignment::Stretch,
                    .children = state.notice.has_value()
                                    ? children(
                                          NoticeBanner {
                                              .notice = *state.notice,
                                              .onOpen = openNoticeTarget,
                                              .onDismiss = dismissNotice,
                                          }
                                              .padding(theme.space3, theme.space3, 0.f, theme.space3),
                                          std::move(currentView)
                                      )
                                    : children(std::move(currentView))
                }
                    .flex(1.f, 1.f)
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
