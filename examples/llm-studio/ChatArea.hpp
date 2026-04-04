#pragma once

#include <Flux.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include "Divider.hpp"
#include "Message.hpp"
#include "MessageBox.hpp"
#include "OllamaClient.hpp"

using namespace flux;
using namespace llm_studio;

namespace {

std::once_flag gOllamaUiHandler;

} // namespace

struct ChatArea : ViewModifiers<ChatArea> {
    auto body() const {
        auto& theme = useEnvironment<Theme>();

        auto messages = useState<std::vector<ChatMessage>>({});
        auto streaming = useState(false);

        std::call_once(gOllamaUiHandler, [&]() {
            Application::instance().eventQueue().on<OllamaUiEvent>([messages, streaming](OllamaUiEvent const& e) {
                auto m = *messages;

                if (e.kind == OllamaUiEvent::Kind::Chunk) {
                    auto role = e.part == OllamaUiEvent::Part::Content ? ChatMessage::Role::Assistant : ChatMessage::Role::Reasoning;

                    if (m.empty() || m.back().role != role) {
                        m.push_back(ChatMessage {role, ""});
                    }

                    m.back().text += e.text;
                }

                if (e.kind == OllamaUiEvent::Kind::Done) {
                    streaming = false;
                }

                messages = std::move(m);
            });
        });

        // UI
        std::vector<Element> messageElements;

        auto messagesValue = *messages;
        for (std::size_t i = 0; i < messagesValue.size(); ++i) {
            messageElements.push_back(
                Message{
                    .message = messagesValue[i]
                }
            );
        }

        return VStack {
            .spacing = 8.f,
            .children = children(
                HStack {
                    .spacing = 8.f,
                    .alignment = Alignment::Center,
                    .children = children(
                        Text {
                            .text = "Chat",
                            .style = theme.typeTitle,
                            .color = theme.colorTextPrimary,
                        }.padding(4.f, 8.f, 4.f, 8.f),
                        Spacer {},
                        Icon {
                            .name = IconName::MoreHoriz,
                            .size = theme.typeTitle.size + 4.f,
                            .weight = 300.f,
                            .color = theme.colorTextPrimary,
                        }.padding(4.f, 8.f, 4.f, 8.f)
                        .cursor(Cursor::Hand)
                    )
                },
                Divider {},
                ScrollView {
                    .axis = ScrollAxis::Vertical,
                    .children = children(
                        VStack {
                            .spacing = 16.f,
                            .children = messageElements
                        }
                    )
                }.flex(1.f),
                MessageBox {
                    .onSend =
                        [messages, streaming](const std::string& message) {
                            if (*streaming) {
                                return;
                            }
                            if (message.empty()) {
                                return;
                            }
                            auto m = *messages;
                            m.push_back(ChatMessage{ChatMessage::Role::User, message});
                            messages = std::move(m);
                            streaming = true;

                            std::vector<ChatMessage> apiMessages = messagesForApi(*messages);
                            nlohmann::json const payload = messagesToJson(apiMessages);

                            startOllamaChatStream(defaultOllamaBaseUrl(), defaultOllamaModel(), std::move(payload),
                                                  [](OllamaUiEvent ev) {
                                                      Application::instance().eventQueue().post(std::move(ev));
                                                  });
                        },
                    .disabled = *streaming
                }
            )
        }
        .fill(FillStyle::solid(Color::hex(0xFFFFFF)))
        .cornerRadius(8.f)
        .shadow(ShadowStyle {
            .radius = 2.f,
            .offset = {0.f, 1.f},
            .color = Color::hex(0xC0C0C0)
        })
        .padding(16.f);
    }
};