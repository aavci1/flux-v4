#include <Flux.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
std::once_flag gLambdaChatEventHandlers;
std::once_flag gLambdaModelEventHandlers;

struct ChatWorkspaceState {
    std::vector<ChatThread> chats = sampleChatThreads();
    int selectedChatIndex = 0;
};

struct LibraryWorkspaceState {
    std::vector<LocalModel> localModels;
    std::vector<DownloadJob> recentDownloadJobs;

    std::string loadedModelPath;
    std::string loadedModelName;
    std::string pendingModelPath;
    std::string pendingModelName;

    bool refreshingModels = false;
    bool downloadingModel = false;
    bool modelLoading = false;

    std::string pendingDownloadJobId;
    std::string pendingDownloadRepoId;
    std::string pendingDownloadFilePath;
};

struct HubWorkspaceState {
    std::string modelSearchQuery;
    std::string modelSearchAuthor;
    RemoteModelSort remoteModelSort = RemoteModelSort::Downloads;
    RemoteModelVisibilityFilter remoteModelVisibility = RemoteModelVisibilityFilter::All;
    std::vector<RemoteModel> remoteModels;
    std::string selectedRemoteRepoId;
    std::vector<RemoteModelFile> selectedRemoteRepoFiles;
    std::optional<RemoteRepoDetail> selectedRemoteRepoDetail;
    bool searchingRemoteModels = false;
    bool loadingRemoteModelFiles = false;
    bool loadingRemoteRepoDetail = false;
};

struct FeedbackWorkspaceState {
    std::optional<AppNotice> notice;
    std::string statusText;
    std::string errorText;
};

std::int64_t steadyNowNanos() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

std::int64_t systemNowMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
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

void setChatModel(ChatWorkspaceState &state, int chatIndex, std::string path, std::string name) {
    if (chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= state.chats.size()) {
        return;
    }
    ChatThread &chat = state.chats[static_cast<std::size_t>(chatIndex)];
    chat.modelPath = std::move(path);
    chat.modelName = std::move(name);
}

std::string currentRemoteSearchKey(HubWorkspaceState const &state) {
    return remoteModelSearchCacheKey(
        state.modelSearchQuery,
        state.modelSearchAuthor,
        state.remoteModelSort,
        state.remoteModelVisibility
    );
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

AppState composeAppState(
    StudioModule currentModule,
    ChatWorkspaceState const &chats,
    LibraryWorkspaceState const &library,
    HubWorkspaceState const &hub,
    FeedbackWorkspaceState const &feedback
) {
    AppState state;
    state.currentModule = currentModule;
    state.chats = chats.chats;
    state.selectedChatIndex = chats.selectedChatIndex;
    state.localModels = library.localModels;
    state.modelSearchQuery = hub.modelSearchQuery;
    state.modelSearchAuthor = hub.modelSearchAuthor;
    state.remoteModelSort = hub.remoteModelSort;
    state.remoteModelVisibility = hub.remoteModelVisibility;
    state.remoteModels = hub.remoteModels;
    state.selectedRemoteRepoId = hub.selectedRemoteRepoId;
    state.selectedRemoteRepoFiles = hub.selectedRemoteRepoFiles;
    state.selectedRemoteRepoDetail = hub.selectedRemoteRepoDetail;
    state.recentDownloadJobs = library.recentDownloadJobs;
    state.loadedModelPath = library.loadedModelPath;
    state.loadedModelName = library.loadedModelName;
    state.pendingModelPath = library.pendingModelPath;
    state.pendingModelName = library.pendingModelName;
    state.notice = feedback.notice;
    state.statusText = feedback.statusText;
    state.errorText = feedback.errorText;
    state.refreshingModels = library.refreshingModels;
    state.searchingRemoteModels = hub.searchingRemoteModels;
    state.loadingRemoteModelFiles = hub.loadingRemoteModelFiles;
    state.loadingRemoteRepoDetail = hub.loadingRemoteRepoDetail;
    state.downloadingModel = library.downloadingModel;
    state.modelLoading = library.modelLoading;
    state.pendingDownloadJobId = library.pendingDownloadJobId;
    state.pendingDownloadRepoId = library.pendingDownloadRepoId;
    state.pendingDownloadFilePath = library.pendingDownloadFilePath;
    return state;
}

AppState composeWorkspaceAppState(
    StudioModule currentModule,
    LibraryWorkspaceState const &library,
    HubWorkspaceState const &hub,
    FeedbackWorkspaceState const &feedback
) {
    return composeAppState(
        currentModule,
        ChatWorkspaceState {
            .chats = {},
            .selectedChatIndex = -1,
        },
        library,
        hub,
        feedback
    );
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

struct ChatsWorkspaceOwner : ViewModifiers<ChatsWorkspaceOwner> {
    bool active = false;
    LibraryWorkspaceState library;
    State<FeedbackWorkspaceState> feedbackState;
    std::function<void(std::string const &, std::string const &)> onRequestModelLoad;

    auto body() const {
        BackendServices &services = backend();
        auto chatsState = useState(ChatWorkspaceState {});
        auto feedbackStateHandle = feedbackState;

        std::call_once(gLambdaChatEventHandlers, [chatsState, feedbackStateHandle]() {
            Application::instance().eventQueue().on<lambda_backend::LlmUiEvent>(
                [chatsState, feedbackStateHandle](lambda_backend::LlmUiEvent const &event) {
                    ChatWorkspaceState nextChats = *chatsState;
                    auto it = std::find_if(nextChats.chats.begin(), nextChats.chats.end(), [&](ChatThread const &chat) {
                        return chat.id == event.chatId;
                    });
                    if (it == nextChats.chats.end()) {
                        return;
                    }

                    FeedbackWorkspaceState nextFeedback = *feedbackStateHandle;
                    bool feedbackChanged = false;
                    std::int64_t const nowNanos = steadyNowNanos();

                    if (event.kind == lambda_backend::LlmUiEvent::Kind::Chunk) {
                        ChatRole const role =
                            event.part == lambda_backend::LlmUiEvent::Part::Thinking ? ChatRole::Reasoning : ChatRole::Assistant;
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
                        it->updatedAt = "now";
                        nextFeedback.statusText = "Response complete";
                        feedbackChanged = true;
                    } else if (event.kind == lambda_backend::LlmUiEvent::Kind::Error) {
                        commitStreamDraft(*it, nowNanos);
                        it->streaming = false;
                        it->messages.push_back(lambda::ChatMessage {
                            .role = ChatRole::Assistant,
                            .text = std::string("[Error] ") + event.text,
                        });
                        syncAssistantParagraphs(it->messages.back());
                        it->updatedAt = "now";
                        nextFeedback.errorText = event.text;
                        feedbackChanged = true;
                    }

                    chatsState = std::move(nextChats);
                    if (feedbackChanged) {
                        feedbackStateHandle = std::move(nextFeedback);
                    }
                }
            );
        });

        if (!active) {
            return Element {Spacer {}}
                .size(0.f, 0.f)
                .flex(1.f, 1.f);
        }

        FeedbackWorkspaceState const feedback = *feedbackStateHandle;
        AppState state = composeAppState(StudioModule::Chats, *chatsState, library, HubWorkspaceState {}, feedback);

        auto selectChatModel = [chatsState, onRequestModelLoad = onRequestModelLoad](
                                   int chatIndex,
                                   std::string const &path,
                                   std::string const &name
                               ) {
            ChatWorkspaceState nextChats = *chatsState;
            setChatModel(nextChats, chatIndex, path, name);
            chatsState = std::move(nextChats);
            if (onRequestModelLoad) {
                onRequestModelLoad(path, name);
            }
        };

        auto sendMessage = [chatsState, feedbackStateHandle, library = library, &services](int chatIndex, std::string const &message) {
            if (message.empty()) {
                return;
            }

            ChatWorkspaceState nextChats = *chatsState;
            if (chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= nextChats.chats.size()) {
                return;
            }

            ChatThread &chat = nextChats.chats[static_cast<std::size_t>(chatIndex)];
            if (chat.streaming || chat.modelPath.empty() || chat.modelPath != library.loadedModelPath || library.modelLoading) {
                return;
            }

            if (chat.messages.empty() || chat.title == "New chat") {
                chat.title = titleFromPrompt(message);
            }

            chat.streamDraftMessages.clear();
            chat.messages.push_back(lambda::ChatMessage {
                .role = ChatRole::User,
                .text = message,
            });
            chat.streaming = true;
            chat.updatedAt = "now";

            FeedbackWorkspaceState nextFeedback = *feedbackStateHandle;
            nextFeedback.errorText.clear();
            nextFeedback.statusText = "Generating response...";

            std::vector<lambda_backend::ChatMessage> history = toBackendMessages(chat);
            std::string streamChatId = chat.id;

            chatsState = std::move(nextChats);
            feedbackStateHandle = std::move(nextFeedback);

            services.engine->startChat(
                std::move(history),
                std::move(streamChatId),
                [](lambda_backend::LlmUiEvent event) {
                    Application::instance().eventQueue().post(std::move(event));
                }
            );
        };

        auto stopMessage = [chatsState, feedbackStateHandle, &services](int chatIndex) {
            services.engine->cancelGeneration();
            ChatWorkspaceState nextChats = *chatsState;
            if (chatIndex >= 0 && static_cast<std::size_t>(chatIndex) < nextChats.chats.size()) {
                nextChats.chats[static_cast<std::size_t>(chatIndex)].streaming = false;
            }
            FeedbackWorkspaceState nextFeedback = *feedbackStateHandle;
            nextFeedback.statusText = "Generation stopped";
            chatsState = std::move(nextChats);
            feedbackStateHandle = std::move(nextFeedback);
        };

        auto toggleReasoningMessage = [chatsState](int chatIndex, int messageIndex) {
            ChatWorkspaceState nextChats = *chatsState;
            if (chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= nextChats.chats.size()) {
                return;
            }
            ChatThread &chat = nextChats.chats[static_cast<std::size_t>(chatIndex)];
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
            if (message == nullptr || message->role != ChatRole::Reasoning) {
                return;
            }
            message->collapsed = !message->collapsed;
            chatsState = std::move(nextChats);
        };

        auto createChat = [chatsState, library = library]() {
            ChatWorkspaceState nextChats = *chatsState;
            ChatThread thread;
            thread.id = lambda::generateChatId();
            thread.title = "New chat";
            thread.updatedAt = "now";
            thread.modelPath = library.loadedModelPath;
            thread.modelName = library.loadedModelName;
            nextChats.chats.push_back(std::move(thread));
            nextChats.selectedChatIndex = static_cast<int>(nextChats.chats.size() - 1);
            chatsState = std::move(nextChats);
        };

        return ChatsView {
            .state = state,
            .onNewChat = createChat,
            .onSelectChat = [chatsState](int index) {
                ChatWorkspaceState nextChats = *chatsState;
                nextChats.selectedChatIndex = index;
                chatsState = std::move(nextChats);
            },
            .onSelectModel = selectChatModel,
            .onSend = sendMessage,
            .onStop = stopMessage,
            .onToggleReasoning = toggleReasoningMessage,
        }
            .flex(1.f, 1.f);
    }
};

struct StudioWorkspaceOwner : ViewModifiers<StudioWorkspaceOwner> {
    State<StudioModule> currentModuleState;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        BackendServices &services = backend();
        auto libraryState = useState(LibraryWorkspaceState {});
        auto hubState = useState(HubWorkspaceState {});
        auto feedbackState = useState(FeedbackWorkspaceState {});
        auto currentModuleHandle = currentModuleState;

        std::call_once(gLambdaModelEventHandlers, [libraryState, hubState, feedbackState, &services]() {
            Application::instance().eventQueue().on<lambda_backend::ModelManagerEvent>(
                [libraryState, hubState, feedbackState, &services](lambda_backend::ModelManagerEvent const &event) {
                    LibraryWorkspaceState nextLibrary = *libraryState;
                    HubWorkspaceState nextHub = *hubState;
                    FeedbackWorkspaceState nextFeedback = *feedbackState;

                    switch (event.kind) {
                    case lambda_backend::ModelManagerEvent::Kind::LocalModelsReady:
                        nextLibrary.refreshingModels = false;
                        nextLibrary.localModels.clear();
                        nextLibrary.localModels.reserve(event.localModels.size());
                        for (lambda_backend::LocalModelInfo const &model : event.localModels) {
                            if (!model.path.empty()) {
                                nextLibrary.localModels.push_back(toLocalModel(model));
                            }
                        }
                        try {
                            services.catalog->replaceLocalModelInstances(nextLibrary.localModels);
                        } catch (std::exception const &e) {
                            nextFeedback.errorText = e.what();
                        }
                        nextFeedback.statusText = nextLibrary.localModels.empty()
                                                      ? "No local models found"
                                                      : "Found " + std::to_string(nextLibrary.localModels.size()) +
                                                            " local model" +
                                                            (nextLibrary.localModels.size() == 1 ? "" : "s");
                        break;
                    case lambda_backend::ModelManagerEvent::Kind::ModelLoaded:
                        nextLibrary.modelLoading = false;
                        nextLibrary.loadedModelPath = event.modelPath;
                        nextLibrary.loadedModelName = event.modelName;
                        nextLibrary.pendingModelPath.clear();
                        nextLibrary.pendingModelName.clear();
                        nextFeedback.notice.reset();
                        nextFeedback.statusText = "Loaded " + event.modelName;
                        nextFeedback.errorText.clear();
                        break;
                    case lambda_backend::ModelManagerEvent::Kind::ModelLoadError:
                        nextLibrary.modelLoading = false;
                        nextLibrary.pendingModelPath.clear();
                        nextLibrary.pendingModelName.clear();
                        nextFeedback.notice.reset();
                        nextFeedback.errorText = event.error;
                        break;
                    case lambda_backend::ModelManagerEvent::Kind::DownloadProgress:
                        if (!nextLibrary.pendingDownloadJobId.empty()) {
                            for (DownloadJob &job : nextLibrary.recentDownloadJobs) {
                                if (job.id != nextLibrary.pendingDownloadJobId) {
                                    continue;
                                }
                                job.downloadedBytes = event.downloadedBytes;
                                job.totalBytes = event.totalBytes;
                                break;
                            }
                        }
                        break;
                    case lambda_backend::ModelManagerEvent::Kind::DownloadDone:
                        nextLibrary.downloadingModel = false;
                        if (!nextLibrary.pendingDownloadJobId.empty()) {
                            for (DownloadJob &job : nextLibrary.recentDownloadJobs) {
                                if (job.id != nextLibrary.pendingDownloadJobId) {
                                    continue;
                                }
                                job.status = DownloadJobStatus::Completed;
                                job.localPath = event.modelPath;
                                job.error.clear();
                                job.finishedAtUnixMs = systemNowMillis();
                                job.downloadedBytes = event.totalBytes;
                                job.totalBytes = event.totalBytes;
                                try {
                                    services.catalog->finishDownloadJob(job.id, job.localPath, job.finishedAtUnixMs);
                                } catch (std::exception const &e) {
                                    nextFeedback.errorText = e.what();
                                }
                                break;
                            }
                        }
                        nextLibrary.pendingDownloadJobId.clear();
                        nextLibrary.pendingDownloadRepoId.clear();
                        nextLibrary.pendingDownloadFilePath.clear();
                        nextFeedback.notice.reset();
                        nextFeedback.statusText = "Downloaded " + event.modelName;
                        nextFeedback.errorText.clear();
                        break;
                    case lambda_backend::ModelManagerEvent::Kind::DownloadError:
                        nextLibrary.downloadingModel = false;
                        if (!nextLibrary.pendingDownloadJobId.empty()) {
                            for (DownloadJob &job : nextLibrary.recentDownloadJobs) {
                                if (job.id != nextLibrary.pendingDownloadJobId) {
                                    continue;
                                }
                                job.status = DownloadJobStatus::Failed;
                                job.error = event.error;
                                job.finishedAtUnixMs = systemNowMillis();
                                if (event.downloadedBytes > 0) {
                                    job.downloadedBytes = event.downloadedBytes;
                                }
                                if (event.totalBytes > 0) {
                                    job.totalBytes = event.totalBytes;
                                }
                                try {
                                    services.catalog->failDownloadJob(job.id, job.error, job.finishedAtUnixMs);
                                } catch (std::exception const &e) {
                                    nextFeedback.errorText = e.what();
                                }
                                break;
                            }
                        }
                        nextLibrary.pendingDownloadJobId.clear();
                        nextLibrary.pendingDownloadRepoId.clear();
                        nextLibrary.pendingDownloadFilePath.clear();
                        nextFeedback.notice.reset();
                        nextFeedback.errorText = event.error;
                        break;
                    case lambda_backend::ModelManagerEvent::Kind::HfSearchReady:
                        nextHub.searchingRemoteModels = false;
                        if (event.searchKey != currentRemoteSearchKey(nextHub)) {
                            if (event.error.empty()) {
                                std::vector<RemoteModel> cachedModels;
                                cachedModels.reserve(event.hfModels.size());
                                for (lambda_backend::HfModelInfo const &model : event.hfModels) {
                                    cachedModels.push_back(toRemoteModel(model));
                                }
                                try {
                                    services.catalog->replaceSearchSnapshot(event.searchKey, cachedModels, event.rawJson);
                                } catch (...) {
                                }
                            }
                            break;
                        }
                        if (!event.error.empty()) {
                            nextFeedback.errorText = event.error;
                            if (nextHub.remoteModels.empty()) {
                                nextHub.selectedRemoteRepoId.clear();
                                nextHub.selectedRemoteRepoFiles.clear();
                                nextHub.selectedRemoteRepoDetail.reset();
                                nextHub.loadingRemoteModelFiles = false;
                                nextHub.loadingRemoteRepoDetail = false;
                            } else {
                                nextFeedback.statusText = "Search refresh failed, showing cached results";
                            }
                        } else {
                            nextFeedback.errorText.clear();
                            nextHub.remoteModels.clear();
                            nextHub.remoteModels.reserve(event.hfModels.size());
                            for (lambda_backend::HfModelInfo const &model : event.hfModels) {
                                nextHub.remoteModels.push_back(toRemoteModel(model));
                            }
                            try {
                                services.catalog->replaceSearchSnapshot(event.searchKey, nextHub.remoteModels, event.rawJson);
                            } catch (std::exception const &e) {
                                nextFeedback.errorText = e.what();
                            }

                            if (nextHub.remoteModels.empty()) {
                                nextHub.selectedRemoteRepoId.clear();
                                nextHub.selectedRemoteRepoFiles.clear();
                                nextHub.selectedRemoteRepoDetail.reset();
                                nextHub.loadingRemoteModelFiles = false;
                                nextHub.loadingRemoteRepoDetail = false;
                                nextFeedback.statusText = "No matching Hugging Face models";
                            } else {
                                bool foundSelection = false;
                                for (RemoteModel const &model : nextHub.remoteModels) {
                                    if (model.id == nextHub.selectedRemoteRepoId) {
                                        foundSelection = true;
                                        break;
                                    }
                                }
                                if (!foundSelection) {
                                    nextHub.selectedRemoteRepoId = nextHub.remoteModels.front().id;
                                    nextHub.selectedRemoteRepoFiles.clear();
                                    nextHub.selectedRemoteRepoDetail.reset();
                                    nextHub.loadingRemoteModelFiles = true;
                                    nextHub.loadingRemoteRepoDetail = true;
                                    services.manager->inspectRepo(nextHub.selectedRemoteRepoId);
                                }
                                nextFeedback.statusText =
                                    "Found " + std::to_string(nextHub.remoteModels.size()) + " matching model" +
                                    (nextHub.remoteModels.size() == 1 ? "" : "s");
                            }
                        }
                        break;
                    case lambda_backend::ModelManagerEvent::Kind::HfFilesReady:
                        if (event.repoId == nextHub.selectedRemoteRepoId) {
                            nextHub.loadingRemoteModelFiles = false;
                        }
                        if (!event.error.empty()) {
                            nextFeedback.errorText = event.error;
                            if (event.repoId == nextHub.selectedRemoteRepoId && !nextHub.selectedRemoteRepoFiles.empty()) {
                                nextFeedback.statusText = "File refresh failed, showing cached repo files";
                            }
                        } else {
                            std::vector<RemoteModelFile> files;
                            files.reserve(event.hfFiles.size());
                            for (lambda_backend::HfFileInfo const &file : event.hfFiles) {
                                files.push_back(toRemoteModelFile(file));
                            }
                            try {
                                services.catalog->replaceRepoFilesSnapshot(event.repoId, files, event.rawJson);
                            } catch (std::exception const &e) {
                                nextFeedback.errorText = e.what();
                            }
                            if (event.repoId == nextHub.selectedRemoteRepoId) {
                                nextFeedback.errorText.clear();
                                nextHub.selectedRemoteRepoFiles = std::move(files);
                                nextFeedback.statusText =
                                    event.hfFiles.empty()
                                        ? "No GGUF files in selected repo"
                                        : "Found " + std::to_string(event.hfFiles.size()) + " GGUF file" +
                                              (event.hfFiles.size() == 1 ? "" : "s");
                            }
                        }
                        break;
                    case lambda_backend::ModelManagerEvent::Kind::HfRepoDetailReady:
                        if (event.repoId == nextHub.selectedRemoteRepoId) {
                            nextHub.loadingRemoteRepoDetail = false;
                        }
                        if (!event.error.empty()) {
                            nextFeedback.errorText = event.error;
                            if (event.repoId == nextHub.selectedRemoteRepoId &&
                                nextHub.selectedRemoteRepoDetail.has_value()) {
                                nextFeedback.statusText = "Repo detail refresh failed, showing cached metadata";
                            }
                        } else {
                            RemoteRepoDetail detail = toRemoteRepoDetail(event.hfRepoDetail);
                            try {
                                services.catalog->replaceRepoDetailSnapshot(detail, event.rawJson);
                            } catch (std::exception const &e) {
                                nextFeedback.errorText = e.what();
                            }
                            if (event.repoId == nextHub.selectedRemoteRepoId) {
                                nextFeedback.errorText.clear();
                                nextHub.selectedRemoteRepoDetail = std::move(detail);
                            }
                        }
                        break;
                    }

                    libraryState = std::move(nextLibrary);
                    hubState = std::move(nextHub);
                    feedbackState = std::move(nextFeedback);
                }
            );

            LibraryWorkspaceState nextLibrary = *libraryState;
            FeedbackWorkspaceState nextFeedback = *feedbackState;
            nextLibrary.refreshingModels = true;
            try {
                services.catalog->markRunningDownloadJobsInterrupted(systemNowMillis());
                nextLibrary.recentDownloadJobs = services.catalog->loadRecentDownloadJobs();
                nextLibrary.localModels = services.catalog->loadLocalModelInstances();
            } catch (std::exception const &e) {
                nextFeedback.errorText = e.what();
            }
            nextLibrary.loadedModelPath = lambda_backend::defaultModelPath();
            nextLibrary.loadedModelName = lambda_backend::defaultModelName();
            if (!nextLibrary.loadedModelPath.empty() && !services.engine->isLoaded()) {
                nextLibrary.modelLoading = true;
                nextLibrary.pendingModelPath = nextLibrary.loadedModelPath;
                nextLibrary.pendingModelName = nextLibrary.loadedModelName;
            }
            libraryState = std::move(nextLibrary);
            feedbackState = std::move(nextFeedback);

            services.manager->refreshLocalModels();
            if (!lambda_backend::defaultModelPath().empty() && !services.engine->isLoaded()) {
                services.manager->loadModel(
                    lambda_backend::defaultModelPath(),
                    lambda_backend::defaultNGpuLayers()
                );
            }
        });

        StudioModule const currentModule = *currentModuleHandle;
        LibraryWorkspaceState const library = *libraryState;
        HubWorkspaceState const hub = *hubState;
        FeedbackWorkspaceState const feedback = *feedbackState;
        AppState workspaceState = composeWorkspaceAppState(currentModule, library, hub, feedback);

        auto requestInventoryRefresh = [libraryState, feedbackState, &services]() {
            LibraryWorkspaceState nextLibrary = *libraryState;
            FeedbackWorkspaceState nextFeedback = *feedbackState;
            nextLibrary.refreshingModels = true;
            nextFeedback.statusText = "Refreshing model inventory...";
            nextFeedback.errorText.clear();
            libraryState = std::move(nextLibrary);
            feedbackState = std::move(nextFeedback);
            services.manager->refreshLocalModels();
        };

        auto requestRemoteSearch = [hubState, feedbackState, &services](
                                       std::string query,
                                       std::string author,
                                       RemoteModelSort sort,
                                       RemoteModelVisibilityFilter visibility
                                   ) {
            HubWorkspaceState nextHub = *hubState;
            FeedbackWorkspaceState nextFeedback = *feedbackState;
            nextHub.modelSearchQuery = query;
            nextHub.modelSearchAuthor = author;
            nextHub.remoteModelSort = sort;
            nextHub.remoteModelVisibility = visibility;
            nextHub.searchingRemoteModels = true;
            std::string const cacheKey = currentRemoteSearchKey(nextHub);
            try {
                nextHub.remoteModels = services.catalog->loadSearchResults(cacheKey);
                if (nextHub.remoteModels.empty()) {
                    nextHub.remoteModels = services.catalog->searchCatalogModels(query, author, sort, visibility);
                }
                nextHub.selectedRemoteRepoId.clear();
                nextHub.selectedRemoteRepoFiles.clear();
                nextHub.selectedRemoteRepoDetail.reset();
                nextHub.loadingRemoteModelFiles = false;
                nextHub.loadingRemoteRepoDetail = false;
                if (!nextHub.remoteModels.empty()) {
                    nextHub.selectedRemoteRepoId = nextHub.remoteModels.front().id;
                    nextHub.selectedRemoteRepoFiles = services.catalog->loadRepoFiles(nextHub.selectedRemoteRepoId);
                    nextHub.selectedRemoteRepoDetail = services.catalog->loadRepoDetail(nextHub.selectedRemoteRepoId);
                    nextHub.loadingRemoteModelFiles = nextHub.selectedRemoteRepoFiles.empty();
                    nextHub.loadingRemoteRepoDetail = !nextHub.selectedRemoteRepoDetail.has_value();
                }
            } catch (std::exception const &e) {
                nextHub.remoteModels.clear();
                nextHub.selectedRemoteRepoId.clear();
                nextHub.selectedRemoteRepoFiles.clear();
                nextHub.selectedRemoteRepoDetail.reset();
                nextHub.loadingRemoteModelFiles = false;
                nextHub.loadingRemoteRepoDetail = false;
                nextFeedback.errorText = e.what();
            }
            nextFeedback.statusText = query.empty() && author.empty() ? "Searching top GGUF models..." :
                                                                      "Searching Hugging Face...";
            hubState = std::move(nextHub);
            feedbackState = std::move(nextFeedback);
            services.manager->searchHuggingFace(lambda_backend::HfSearchRequest {
                .query = std::move(query),
                .author = std::move(author),
                .sortKey = sort == RemoteModelSort::Likes   ? "likes" :
                           sort == RemoteModelSort::Updated ? "lastModified" :
                                                              "downloads",
                .visibilityFilter = visibility == RemoteModelVisibilityFilter::PublicOnly ? "public" :
                                    visibility == RemoteModelVisibilityFilter::GatedOnly  ? "gated" :
                                                                                            "all",
                .cacheKey = cacheKey,
            });
        };

        auto requestRemoteRepoFiles = [hubState, feedbackState, &services](std::string repoId) {
            if (repoId.empty()) {
                return;
            }
            HubWorkspaceState nextHub = *hubState;
            FeedbackWorkspaceState nextFeedback = *feedbackState;
            nextHub.selectedRemoteRepoId = repoId;
            try {
                nextHub.selectedRemoteRepoFiles = services.catalog->loadRepoFiles(repoId);
                nextHub.selectedRemoteRepoDetail = services.catalog->loadRepoDetail(repoId);
                nextHub.loadingRemoteModelFiles = nextHub.selectedRemoteRepoFiles.empty();
                nextHub.loadingRemoteRepoDetail = !nextHub.selectedRemoteRepoDetail.has_value();
                nextFeedback.errorText.clear();
            } catch (std::exception const &e) {
                nextHub.selectedRemoteRepoFiles.clear();
                nextHub.selectedRemoteRepoDetail.reset();
                nextHub.loadingRemoteModelFiles = true;
                nextHub.loadingRemoteRepoDetail = true;
                nextFeedback.errorText = e.what();
            }
            nextFeedback.statusText = "Loading GGUF files...";
            hubState = std::move(nextHub);
            feedbackState = std::move(nextFeedback);
            services.manager->inspectRepo(std::move(repoId));
        };

        auto requestRemoteDownload = [libraryState, feedbackState, &services](std::string repoId, std::string path) {
            if (repoId.empty() || path.empty()) {
                return;
            }
            LibraryWorkspaceState nextLibrary = *libraryState;
            FeedbackWorkspaceState nextFeedback = *feedbackState;
            DownloadJob job;
            job.id = repoId + "|" + path + "|" + std::to_string(systemNowMillis());
            job.repoId = repoId;
            job.filePath = path;
            job.startedAtUnixMs = systemNowMillis();
            job.status = DownloadJobStatus::Running;
            nextLibrary.recentDownloadJobs.insert(nextLibrary.recentDownloadJobs.begin(), job);
            if (nextLibrary.recentDownloadJobs.size() > 12) {
                nextLibrary.recentDownloadJobs.resize(12);
            }
            nextLibrary.downloadingModel = true;
            nextLibrary.pendingDownloadJobId = job.id;
            nextLibrary.pendingDownloadRepoId = repoId;
            nextLibrary.pendingDownloadFilePath = path;
            nextFeedback.notice = AppNotice {
                .title = "Download started",
                .detail = path + " is downloading. Follow progress in the Models view.",
                .targetModule = StudioModule::Models,
            };
            nextFeedback.statusText = "Downloading " + path;
            nextFeedback.errorText.clear();
            try {
                services.catalog->startDownloadJob(job);
            } catch (std::exception const &e) {
                nextFeedback.errorText = e.what();
            }
            libraryState = std::move(nextLibrary);
            feedbackState = std::move(nextFeedback);
            services.manager->downloadModel(std::move(repoId), std::move(path));
        };

        auto requestModelLoad = [libraryState, feedbackState, &services](std::string const &path, std::string const &name) {
            if (path.empty()) {
                return;
            }
            LibraryWorkspaceState nextLibrary = *libraryState;
            FeedbackWorkspaceState nextFeedback = *feedbackState;
            nextLibrary.modelLoading = true;
            nextLibrary.pendingModelPath = path;
            nextLibrary.pendingModelName = name;
            nextFeedback.statusText = "Loading " + name;
            nextFeedback.errorText.clear();
            libraryState = std::move(nextLibrary);
            feedbackState = std::move(nextFeedback);
            services.manager->loadModel(path, lambda_backend::defaultNGpuLayers());
        };

        std::vector<Element> contentLayers;
        contentLayers.reserve(2);
        contentLayers.push_back(
            ChatsWorkspaceOwner {
                .active = currentModule == StudioModule::Chats,
                .library = library,
                .feedbackState = feedbackState,
                .onRequestModelLoad = requestModelLoad,
            }
                .flex(1.f, 1.f)
        );

        if (currentModule == StudioModule::Models) {
            contentLayers.push_back(
                ModelsView {
                    .state = workspaceState,
                    .onRefresh = requestInventoryRefresh,
                    .onLoad = requestModelLoad,
                    .onRetryDownload = requestRemoteDownload,
                }
                    .flex(1.f, 1.f)
            );
        } else if (currentModule == StudioModule::Hub) {
            contentLayers.push_back(
                HubView {
                    .state = workspaceState,
                    .onSearchQueryChange = [hubState](std::string const &query) {
                        HubWorkspaceState nextHub = *hubState;
                        nextHub.modelSearchQuery = query;
                        hubState = std::move(nextHub);
                    },
                    .onSearchAuthorChange = [hubState](std::string const &author) {
                        HubWorkspaceState nextHub = *hubState;
                        nextHub.modelSearchAuthor = author;
                        hubState = std::move(nextHub);
                    },
                    .onSortChange = [hubState](RemoteModelSort sort) {
                        HubWorkspaceState nextHub = *hubState;
                        nextHub.remoteModelSort = sort;
                        hubState = std::move(nextHub);
                    },
                    .onVisibilityChange = [hubState](RemoteModelVisibilityFilter visibility) {
                        HubWorkspaceState nextHub = *hubState;
                        nextHub.remoteModelVisibility = visibility;
                        hubState = std::move(nextHub);
                    },
                    .onSearch = requestRemoteSearch,
                    .onSelectRemoteRepo = requestRemoteRepoFiles,
                    .onDownload = requestRemoteDownload,
                }
                    .flex(1.f, 1.f)
            );
        } else if (currentModule == StudioModule::Settings) {
            contentLayers.push_back(
                SettingsView {
                    .state = workspaceState,
                }
                    .flex(1.f, 1.f)
            );
        }

        Element content = Element {ZStack {
            .horizontalAlignment = Alignment::Start,
            .verticalAlignment = Alignment::Start,
            .children = std::move(contentLayers),
        }}
                              .flex(1.f, 1.f);

        return VStack {
            .spacing = theme.space3,
            .alignment = Alignment::Stretch,
            .children = feedback.notice.has_value()
                            ? children(
                                  NoticeBanner {
                                      .notice = *feedback.notice,
                                      .onOpen = [feedbackState, currentModuleHandle] {
                                          FeedbackWorkspaceState nextFeedback = *feedbackState;
                                          if (nextFeedback.notice.has_value()) {
                                              currentModuleHandle = nextFeedback.notice->targetModule;
                                          }
                                          nextFeedback.notice.reset();
                                          feedbackState = std::move(nextFeedback);
                                      },
                                      .onDismiss = [feedbackState] {
                                          FeedbackWorkspaceState nextFeedback = *feedbackState;
                                          nextFeedback.notice.reset();
                                          feedbackState = std::move(nextFeedback);
                                      },
                                  }
                                      .padding(theme.space3, theme.space3, 0.f, theme.space3),
                                  std::move(content)
                              )
                            : children(std::move(content))
        }
            .flex(1.f, 1.f);
    }
};

struct LambdaStudio : ViewModifiers<LambdaStudio> {
    auto body() const {
        auto currentModule = useState(StudioModule::Chats);

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
                    .selectedTitle = moduleTitle(*currentModule),
                    .onSelect = [currentModule](std::string title) {
                        currentModule = moduleFromTitle(title);
                    },
                }
                    .flex(0.f, 0.f),
                Divider {.orientation = Divider::Orientation::Vertical},
                StudioWorkspaceOwner {
                    .currentModuleState = currentModule,
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
