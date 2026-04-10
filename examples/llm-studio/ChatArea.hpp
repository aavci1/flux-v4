#pragma once

#include <Flux.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "Divider.hpp"
#include "MessageBubble.hpp"
#include "MessageEditor.hpp"
#include "LlamaEngine.hpp"
#include "Types.hpp"

using namespace flux;
using namespace llm_studio;

struct ChatArea : ViewModifiers<ChatArea> {
    std::optional<Chat> chat;
    std::string currentModelName;
    std::vector<PickerOption<std::string>> availableModels;
    std::function<void(const std::string&, const std::string&)> onSend;
    std::function<void(std::string const&)> onChangeModel;

    auto body() const {
        auto& theme = useEnvironment<Theme>();
        auto streaming = useState(false);
        // Must always be declared: conditional useState corrupts Flux StateStore slot order.
        auto modelPickerValue = useState<std::string>(currentModelName);

        // MessageEditor must always be mounted (same hooks every frame). Nesting it only in the
        // `chat` branch changes hook counts when a chat is selected and corrupts TextInput/StateStore.
        std::vector<Element> stackChildren;

        if (!chat) {
            stackChildren.push_back(Element {
                Text {
                    .text = "No chat selected",
                    .font = theme.fontHeading,
                    .color = theme.colorTextMuted,
                    .horizontalAlignment = HorizontalAlignment::Center,
                    .verticalAlignment = VerticalAlignment::Center,
                }
                .flex(1.f)
            });
        } else {
            std::vector<Element> messageElements;
            for (auto &message : chat->messages) {
                messageElements.push_back(
                    MessageBubble {
                        .message = message
                    }
                );
            }

            std::vector<Element> headerChildren;
            headerChildren.push_back(Element {
                Text {
                    .text = "Chat",
                    .font = theme.fontTitle,
                    .color = theme.colorTextPrimary,
                }.padding(4.f, 8.f, 4.f, 8.f)
            });

            // Always use Picker (not Text when empty): switching components changes nested hook counts.
            auto changeFn = onChangeModel;
            headerChildren.push_back(Element {
                Picker<std::string> {
                    .value = modelPickerValue,
                    .options = availableModels,
                    .placeholder = currentModelName.empty() ? "No model" : currentModelName,
                    .onChange = [changeFn](std::string const& path) {
                        if (changeFn) changeFn(path);
                    },
                }
            });

            headerChildren.push_back(Element { Spacer {} });
            headerChildren.push_back(Element {
                Icon {
                    .name = IconName::MoreHoriz,
                    .size = theme.fontTitle.size + 4.f,
                    .weight = 300.f,
                    .color = theme.colorTextPrimary,
                }.padding(4.f, 8.f, 4.f, 8.f)
                .cursor(Cursor::Hand)
            });

            stackChildren.push_back(Element {
                HStack {
                    .spacing = 8.f,
                    .alignment = Alignment::Center,
                    .children = headerChildren,
                }
            });
            stackChildren.push_back(Element { Divider{} });
            stackChildren.push_back(Element {
                ScrollView {
                    .axis = ScrollAxis::Vertical,
                    .children = children(
                        VStack {
                            .spacing = 16.f,
                            .children = messageElements
                        }
                    )
                }.flex(1.f)
            });
        }

        std::optional<Chat> const chatCopy = chat;
        auto const sendFn = onSend;
        stackChildren.push_back(Element {
            MessageEditor {
                .modelName = chatCopy ? chatCopy->modelName : std::string{},
                .onSend = [chatCopy, sendFn](std::string const& message) {
                    if (message.empty() || !chatCopy) {
                        return;
                    }
                    if (sendFn) {
                        sendFn(chatCopy->modelName, message);
                    }
                },
                .disabled = !chatCopy.has_value() || *streaming
            }
        });

        return VStack {
            .spacing = 8.f,
            .alignment = Alignment::Start,
            .children = std::move(stackChildren),
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
