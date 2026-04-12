#include <Flux.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
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

void setChatModel(AppState &state, int chatIndex, std::string path, std::string name) {
    if (chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= state.chats.size()) {
        return;
    }
    ChatThread &chat = state.chats[static_cast<std::size_t>(chatIndex)];
    chat.modelPath = std::move(path);
    chat.modelName = std::move(name);
}

std::vector<::ChatMessage> toBackendMessages(ChatThread const &chat) {
    std::vector<::ChatMessage> result;
    result.reserve(chat.messages.size());
    for (lambda::ChatMessage const &message : chat.messages) {
        result.push_back(::ChatMessage {
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
            Application::instance().eventQueue().on<llm_studio::LlmUiEvent>(
                [appState](llm_studio::LlmUiEvent const &event) {
                    AppState nextState = *appState;
                    auto it = std::find_if(nextState.chats.begin(), nextState.chats.end(), [&](ChatThread const &chat) {
                        return chat.id == event.chatId;
                    });
                    if (it == nextState.chats.end()) {
                        return;
                    }

                    if (event.kind == llm_studio::LlmUiEvent::Kind::Chunk) {
                        ChatRole const role = event.part == llm_studio::LlmUiEvent::Part::Thinking ? ChatRole::Reasoning :
                                                                                                     ChatRole::Assistant;
                        if (it->messages.empty() || it->messages.back().role != role) {
                            it->messages.push_back(lambda::ChatMessage {.role = role, .text = ""});
                        }
                        it->messages.back().text += event.text;
                    } else if (event.kind == llm_studio::LlmUiEvent::Kind::Done) {
                        it->streaming = false;
                        it->updatedAt = "now";
                        nextState.statusText = "Response complete";
                    } else if (event.kind == llm_studio::LlmUiEvent::Kind::Error) {
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

            Application::instance().eventQueue().on<ModelManagerEvent>(
                [appState](ModelManagerEvent const &event) {
                    AppState nextState = *appState;
                    switch (event.kind) {
                    case ModelManagerEvent::Kind::LocalModelsReady:
                        nextState.refreshingModels = false;
                        nextState.localModels.clear();
                        nextState.localModels.reserve(event.localModels.size());
                        for (LocalModelInfo const &model : event.localModels) {
                            if (!model.path.empty()) {
                                nextState.localModels.push_back(toLocalModel(model));
                            }
                        }
                        nextState.statusText = nextState.localModels.empty() ? "No local models found" : "Model inventory refreshed";
                        break;
                    case ModelManagerEvent::Kind::ModelLoaded:
                        nextState.modelLoading = false;
                        nextState.loadedModelPath = event.modelPath;
                        nextState.loadedModelName = event.modelName;
                        nextState.pendingModelPath.clear();
                        nextState.pendingModelName.clear();
                        nextState.statusText = "Loaded " + event.modelName;
                        nextState.errorText.clear();
                        break;
                    case ModelManagerEvent::Kind::ModelLoadError:
                        nextState.modelLoading = false;
                        nextState.pendingModelPath.clear();
                        nextState.pendingModelName.clear();
                        nextState.errorText = event.error;
                        break;
                    case ModelManagerEvent::Kind::DownloadDone:
                    case ModelManagerEvent::Kind::DownloadError:
                    case ModelManagerEvent::Kind::HfSearchReady:
                    case ModelManagerEvent::Kind::HfFilesReady:
                        break;
                    }
                    appState = std::move(nextState);
                }
            );

            AppState nextState = *appState;
            nextState.refreshingModels = true;
            nextState.loadedModelPath = llm_studio::defaultModelPath();
            nextState.loadedModelName = llm_studio::defaultModelName();
            if (!nextState.loadedModelPath.empty() && !services.engine->isLoaded()) {
                nextState.modelLoading = true;
                nextState.pendingModelPath = nextState.loadedModelPath;
                nextState.pendingModelName = nextState.loadedModelName;
            }
            appState = std::move(nextState);

            services.manager->refreshLocalModels();
            if (!llm_studio::defaultModelPath().empty() && !services.engine->isLoaded()) {
                services.manager->loadModel(llm_studio::defaultModelPath(), llm_studio::defaultNGpuLayers());
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
            services.manager->loadModel(path, llm_studio::defaultNGpuLayers());
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

            std::vector<::ChatMessage> history = toBackendMessages(chat);
            std::string streamChatId = chat.id;

            appState = std::move(nextState);

            services.engine->startChat(
                std::move(history),
                std::move(streamChatId),
                [](llm_studio::LlmUiEvent event) {
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
