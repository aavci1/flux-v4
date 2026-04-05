#pragma once

#include <Flux.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include "Divider.hpp"
#include "MessageBubble.hpp"
#include "MessageEditor.hpp"
#include "OllamaClient.hpp"
#include "Types.hpp"

using namespace flux;
using namespace llm_studio;

namespace {

std::once_flag gOllamaUiHandler;

} // namespace

struct ChatArea : ViewModifiers<ChatArea> {
    Chat chat;
    std::function<void(const std::string&, const std::string&)> onSend;

    auto body() const {
        auto& theme = useEnvironment<Theme>();
        auto streaming = useState(false);

        // UI
        std::vector<Element> messageElements;
        for (auto &message : chat.messages) {
            messageElements.push_back(
                MessageBubble {
                    .message = message
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
                MessageEditor {
                    .modelName = chat.modelName,
                    .onSend = [modelName = chat.modelName,sendHandler = onSend](const std::string& message) {
                        if (message.empty()) {
                            return;
                        }

                        if (sendHandler) {
                            sendHandler(modelName, message);
                        }
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