#include <Flux.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "ChatArea.hpp"
#include "ChatList.hpp"
#include "Divider.hpp"
#include "MessageEditor.hpp"
#include "LlamaEngine.hpp"
#include "ModelBrowser.hpp"
#include "ModelManager.hpp"
#include "PropertiesPanel.hpp"

using namespace flux;
using namespace llm_studio;

static std::shared_ptr<LlamaEngine>  gEngine  = std::make_shared<LlamaEngine>();
static std::shared_ptr<ModelManager> gManager;

namespace {
std::once_flag gEventHandlers;
}

struct AppRoot : ViewModifiers<AppRoot> {
    auto body() const {
        auto modelPath   = useState<std::string>(defaultModelPath());
        auto modelName   = useState<std::string>(defaultModelName());
        auto chats       = useState<std::vector<Chat>>({});
        auto index       = useState<size_t>(0);

        auto localModels = useState<std::vector<LocalModelInfo>>({});
        auto hfResults   = useState<std::vector<HfModelInfo>>({});
        auto hfFiles     = useState<std::vector<HfFileInfo>>({});
        auto downloading = useState<bool>(false);

        auto temperature = useState<float>(0.80f);
        auto topP        = useState<float>(0.95f);
        auto topK        = useState<float>(40.f);
        auto maxTokens   = useState<float>(4096.f);

        std::call_once(gEventHandlers, [&]() {
            // ── LLM streaming events ────────────────────────────────────
            Application::instance().eventQueue().on<LlmUiEvent>([chats](LlmUiEvent const& e) {
                auto c = *chats;
                auto it = std::find_if(c.begin(), c.end(),
                    [&](Chat const& ch) { return ch.id == e.chatId; });
                if (it == c.end()) return;

                auto chat = *it;

                if (e.kind == LlmUiEvent::Kind::Chunk) {
                    auto role = e.part == LlmUiEvent::Part::Content
                        ? ChatMessage::Role::Assistant
                        : ChatMessage::Role::Reasoning;

                    if (chat.messages.empty() || chat.messages.back().role != role) {
                        chat.messages.push_back(ChatMessage{role, ""});
                    }
                    chat.messages.back().text += e.text;
                }

                if (e.kind == LlmUiEvent::Kind::Done) {
                    chat.streaming = false;
                }

                if (e.kind == LlmUiEvent::Kind::Error) {
                    chat.streaming = false;
                    chat.messages.push_back(ChatMessage{
                        ChatMessage::Role::Assistant,
                        std::string("[Error] ") + e.text,
                    });
                }

                *it = chat;
                chats = c;
            });

            // ── Model manager events ────────────────────────────────────
            Application::instance().eventQueue().on<ModelManagerEvent>(
                [localModels, hfResults, hfFiles, modelPath, modelName, downloading]
                (ModelManagerEvent const& e) {
                    switch (e.kind) {
                    case ModelManagerEvent::Kind::LocalModelsReady:
                        localModels = e.localModels;
                        break;
                    case ModelManagerEvent::Kind::HfSearchReady:
                        hfResults = e.hfModels;
                        break;
                    case ModelManagerEvent::Kind::HfFilesReady:
                        hfFiles = e.hfFiles;
                        break;
                    case ModelManagerEvent::Kind::DownloadDone:
                        downloading = false;
                        std::fprintf(stderr, "[LLM Studio] Download complete: %s\n", e.modelPath.c_str());
                        break;
                    case ModelManagerEvent::Kind::DownloadError:
                        downloading = false;
                        std::fprintf(stderr, "[LLM Studio] Download error: %s\n", e.error.c_str());
                        break;
                    case ModelManagerEvent::Kind::ModelLoaded:
                        modelPath = e.modelPath;
                        modelName = e.modelName;
                        std::fprintf(stderr, "[LLM Studio] Model loaded: %s\n", e.modelName.c_str());
                        break;
                    case ModelManagerEvent::Kind::ModelLoadError:
                        std::fprintf(stderr, "[LLM Studio] Load error: %s\n", e.error.c_str());
                        break;
                    }
                }
            );

            // Load model from env var if present
            std::string path = *modelPath;
            if (!path.empty() && !gEngine->isLoaded()) {
                gManager->loadModel(path, defaultNGpuLayers());
            }

            gManager->refreshLocalModels();
        });

        // ── Sync sampling params to engine when they change ─────────
        SamplingParams sp {
            .temp      = *temperature,
            .topP      = *topP,
            .topK      = static_cast<int32_t>(*topK),
            .maxTokens = static_cast<int32_t>(*maxTokens),
        };
        gEngine->setSamplingParams(sp);

        auto c = *chats;
        auto i = *index;

        // Build available-model list for the chat header picker
        std::vector<PickerOption<std::string>> availModels;
        for (auto const& m : *localModels) {
            if (!m.path.empty()) {
                availModels.push_back({m.path, m.displayName()});
            }
        }

        auto element = ChatArea {
            .chat = c.size() > i ? std::optional<Chat>(c[i]) : std::nullopt,
            .currentModelName = *modelName,
            .availableModels = availModels,
            .onSend = [chats, index](const std::string& /*modelName*/, const std::string& message) {
                auto c = *chats;
                auto i = *index;

                if (c[i].streaming) return;

                c[i].messages.push_back(ChatMessage{ChatMessage::Role::User, message});
                c[i].streaming = true;

                std::vector<ChatMessage> history = c[i].messages;
                std::string const streamChatId = c[i].id;

                std::move(chats) = std::move(c);

                gEngine->startChat(
                    std::move(history),
                    streamChatId,
                    [](LlmUiEvent ev) {
                        Application::instance().eventQueue().post(std::move(ev));
                    }
                );
            },
            .onChangeModel = [](std::string const& path) {
                gManager->loadModel(path);
            },
        }.flex(1.f, 1.f, 400.f);

        return HStack {
            .spacing = 16.f,
            .alignment = Alignment::Stretch,
            .children = children(
                ChatList {
                    .chats = c,
                    .selectedIndex = i,
                    .onChatSelected = [index](size_t selected) {
                        std::move(index) = selected;
                    },
                    .onNewChat = [modelName, chats, index]() {
                        auto c = *chats;
                        Chat fresh{};
                        fresh.modelName = *modelName;
                        fresh.id = generateChatId();
                        fresh.title = std::string("Chat ") + std::to_string(c.size() + 1);
                        c.push_back(std::move(fresh));
                        std::move(index) = std::move(c.size() - 1);
                        std::move(chats) = std::move(c);
                    }
                },
                element,
                VStack {
                    .spacing = 0.f,
                    .children = children(
                        ModelBrowser {
                            .localModels = *localModels,
                            .hfResults = *hfResults,
                            .hfFiles = *hfFiles,
                            .activeModelPath = *modelPath,
                            .downloading = *downloading,
                            .onRefreshLocal = []() {
                                gManager->refreshLocalModels();
                            },
                            .onSearch = [](std::string const& query) {
                                gManager->searchHuggingFace(query);
                            },
                            .onSelectRepo = [](std::string const& repoId) {
                                gManager->listRepoFiles(repoId);
                            },
                            .onDownload = [downloading](std::string const& repo, std::string const& file) {
                                downloading = true;
                                gManager->downloadModel(repo, file);
                            },
                            .onLoadLocal = [](std::string const& path) {
                                gManager->loadModel(path);
                            },
                        }.flex(1.f),
                        PropertiesPanel {
                            .modelPath = *modelPath,
                            .modelName = *modelName,
                            .temperature = temperature,
                            .topP = topP,
                            .topK = topK,
                            .maxTokens = maxTokens,
                        }
                    )
                }.size(320.f, 0.f)
            ),
        }.padding(16.f);
    }
};

int main(int argc, char* argv[]) {
    Application app(argc, argv);

    gManager = std::make_shared<ModelManager>(
        gEngine,
        [](ModelManagerEvent ev) {
            Application::instance().eventQueue().post(std::move(ev));
        }
    );

    auto& w = app.createWindow<Window>({
        .size = {1280, 800},
        .title = "LLM Studio",
    });

    w.setView(AppRoot{});

    int const code = app.exec();

    gManager.reset();
    gEngine.reset();
    llama_backend_free();

    return code;
}
