#include <Flux.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <string>

#include "ChatArea.hpp"
#include "ChatList.hpp"
#include "Divider.hpp"
#include "MessageEditor.hpp"
#include "OllamaClient.hpp"
#include "PropertiesPanel.hpp"

using namespace flux;

struct AppRoot : ViewModifiers<AppRoot> {
    auto body() const {
        auto host = useState<std::string>(defaultOllamaBaseUrl());
        auto model = useState<std::string>(defaultOllamaModel());
        auto chats = useState<std::vector<Chat>>({});
        auto index = useState<size_t>(0);

        std::call_once(gOllamaUiHandler, [&]() {
            Application::instance().eventQueue().on<OllamaUiEvent>([chats](OllamaUiEvent const& e) {
                auto c = *chats;
                auto it = std::find_if(c.begin(), c.end(), [&](Chat const& ch) { return ch.id == e.chatId; });
                if (it == c.end()) {
                    return;
                }

                auto chat = *it;

                if (e.kind == OllamaUiEvent::Kind::Chunk) {
                    auto role = e.part == OllamaUiEvent::Part::Content ? ChatMessage::Role::Assistant : ChatMessage::Role::Reasoning;

                    if (chat.messages.empty() || chat.messages.back().role != role) {
                        chat.messages.push_back(ChatMessage {role, ""});
                    }

                    chat.messages.back().text += e.text;
                }

                if (e.kind == OllamaUiEvent::Kind::Done) {
                    chat.streaming = false;
                }

                if (e.kind == OllamaUiEvent::Kind::Error) {
                    chat.streaming = false;
                }

                *it = chat;
                chats = c;
            });
        });

        auto c = *chats;
        auto i = *index;

        auto element = i >= c.size() ? Rectangle {}.flex(1.f, 1.f, 400.f) : ChatArea {
            .chat = c[i],
            .onSend = [host, chats, index](const std::string& modelName, const std::string& message) {
                auto c = *chats;
                auto i = *index;

                if (c[i].streaming) {
                    return;
                }

                c[i].messages.push_back(ChatMessage {ChatMessage::Role::User, message});
                c[i].streaming = true;

                // Must build the request from `c` before `std::move(c)` into state — otherwise `c` is moved-from (UB).
                std::vector<ChatMessage> apiMessages = messagesForApi(c[i].messages);
                nlohmann::json payload = messagesToJson(apiMessages);
                std::string const streamChatId = c[i].id;

                std::move(chats) = std::move(c);

                startOllamaChatStream(
                    *host,
                    modelName,
                    std::move(payload),
                    streamChatId,
                    [](OllamaUiEvent ev) {
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
                    .onNewChat = [model, chats, index]() {
                        auto c = *chats;
                        Chat fresh {};
                        fresh.modelName = *model;
                        fresh.id = generateChatId();
                        fresh.title = std::string("Chat ") + std::to_string(c.size() + 1);
                        c.push_back(std::move(fresh));

                        std::move(index) = std::move(c.size() - 1);
                        std::move(chats) = std::move(c);
                    }
                },
                element,
                PropertiesPanel {
                    .host = host,
                    .model = model,
                }
            ),
        }.padding(16.f);
    }
};

int main(int argc, char* argv[]) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    Application app(argc, argv);

    auto& w = app.createWindow<Window>({
        .size = {1280, 800},
        .title = "LLM Studio",
    });

    w.setView(AppRoot {});

    int const code = app.exec();
    curl_global_cleanup();
    return code;
}
