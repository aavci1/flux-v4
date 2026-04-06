#include <Flux.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <thread>

#include "ChatArea.hpp"
#include "ChatList.hpp"
#include "Divider.hpp"
#include "MessageEditor.hpp"
#include "LlamaEngine.hpp"
#include "PropertiesPanel.hpp"

using namespace flux;
using namespace llm_studio;

// Global engine — lives for the process lifetime, shared across chats
static std::shared_ptr<LlamaEngine> gEngine = std::make_shared<LlamaEngine>();

namespace {
std::once_flag gLlmUiHandler;
}

struct AppRoot : ViewModifiers<AppRoot> {
    auto body() const {
        auto modelPath = useState<std::string>(defaultModelPath());
        auto modelName = useState<std::string>(defaultModelName());
        auto chats = useState<std::vector<Chat>>({});
        auto index = useState<size_t>(0);

        std::call_once(gLlmUiHandler, [&]() {
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

            // Load model if path was provided via env var
            std::string path = *modelPath;
            if (!path.empty() && !gEngine->isLoaded()) {
                std::thread([path]() {
                    gEngine->load(path, defaultNGpuLayers());
                }).detach();
            }
        });

        auto c = *chats;
        auto i = *index;

        auto element = ChatArea {
            .chat = c.size() > i ? std::optional<Chat>(c[i]) : std::nullopt,
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
            }
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
                PropertiesPanel {
                    .host = gEngine->isLoaded() ? gEngine->modelPath() : "No model loaded",
                    .model = *modelName,
                }
            ),
        }.padding(16.f);
    }
};

int main(int argc, char* argv[]) {
    Application app(argc, argv);

    auto& w = app.createWindow<Window>({
        .size = {1280, 800},
        .title = "LLM Studio",
    });

    w.setView(AppRoot{});

    int const code = app.exec();

    // Engine must be destroyed before llama_backend_free
    gEngine.reset();
    llama_backend_free();

    return code;
}
