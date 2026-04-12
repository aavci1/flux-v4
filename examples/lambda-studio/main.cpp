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

        std::call_once(gLambdaEventHandlers, [appState, &services]() {
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
                [appState](lambda_backend::ModelManagerEvent const &event) {
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
                        nextState.statusText = nextState.localModels.empty() ? "No local models found" : "Model inventory refreshed";
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
                    case lambda_backend::ModelManagerEvent::Kind::DownloadError:
                    case lambda_backend::ModelManagerEvent::Kind::HfSearchReady:
                    case lambda_backend::ModelManagerEvent::Kind::HfFilesReady:
                        break;
                    }
                    appState = std::move(nextState);
                }
            );

            AppState nextState = *appState;
            nextState.refreshingModels = true;
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
