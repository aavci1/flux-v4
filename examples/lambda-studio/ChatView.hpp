#pragma once

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <functional>
#include <string>
#include <vector>

#include "ChatModels.hpp"

using namespace flux;

namespace lambda {

struct ChatBubble : ViewModifiers<ChatBubble> {
    ChatMessage message;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        bool const isUser = message.role == ChatRole::User;
        Color const fill = isUser ? theme.colorAccent : theme.colorSurfaceOverlay;
        Color const textColor = isUser ? theme.colorTextOnAccent : theme.colorTextPrimary;
        std::string const label = isUser ? "You" : "Lambda";

        Element bubble = VStack {
            .spacing = theme.space1,
            .alignment = Alignment::Start,
            .children = children(
                Text {
                    .text = label,
                    .font = theme.fontLabelSmall,
                    .color = isUser ? theme.colorTextOnAccent : theme.colorTextSecondary,
                    .horizontalAlignment = HorizontalAlignment::Leading,
                },
                Text {
                    .text = message.text,
                    .font = theme.fontBody,
                    .color = textColor,
                    .horizontalAlignment = HorizontalAlignment::Leading,
                    .verticalAlignment = VerticalAlignment::Top,
                    .wrapping = TextWrapping::Wrap,
                }
            ),
        }
                             .padding(theme.space4)
                             .fill(FillStyle::solid(fill))
                             .stroke(isUser ? StrokeStyle::none() : StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
                             .cornerRadius(theme.radiusXLarge);

        if (isUser) {
            return HStack {
                .spacing = theme.space3,
                .alignment = Alignment::Start,
                .children = children(
                    Spacer {},
                    std::move(bubble)
                ),
            };
        }

        return HStack {
            .spacing = theme.space3,
            .alignment = Alignment::Start,
            .children = children(
                std::move(bubble),
                Spacer {}
            ),
        };
    }
};

struct ChatComposer : ViewModifiers<ChatComposer> {
    State<std::string> value;
    std::function<void(std::string const &)> onSend;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        auto draftState = value;
        bool const canSend = !(*draftState).empty();
        auto submit = [draftState, onSend = onSend]() {
            std::string message = *draftState;
            if (message.empty() || !onSend) {
                return;
            }
            onSend(message);
            draftState = "";
        };

        return VStack {
            .spacing = theme.space3,
            .alignment = Alignment::Start,
            .children = children(
                TextInput {
                    .value = value,
                    .placeholder = "Message Lambda Studio...",
                    .style = TextInput::Style::plain(),
                    .multiline = true,
                    .multilineHeight = {.minIntrinsic = 88.f, .maxIntrinsic = 160.f},
                    .onSubmit = [submit](std::string const &) {
                        submit();
                    },
                },
                HStack {
                    .spacing = theme.space3,
                    .alignment = Alignment::Center,
                    .children = children(Icon {
                                             .name = IconName::Add,
                                             .size = 20.f,
                                             .weight = 500.f,
                                             .color = theme.colorTextSecondary,
                                         }
                                             .padding(theme.space2)
                                             .fill(FillStyle::solid(theme.colorSurface))
                                             .cornerRadius(theme.radiusFull)
                                             .cursor(Cursor::Hand),
                                         Text {
                                             .text = "Local draft",
                                             .font = theme.fontBodySmall,
                                             .color = theme.colorTextSecondary,
                                         },
                                         Spacer {}, Icon {
                                                        .name = IconName::ArrowUpward,
                                                        .size = 16.f,
                                                        .weight = 700.f,
                                                        .color = canSend ? theme.colorTextOnAccent : theme.colorTextMuted,
                                                    }
                                                        .padding(theme.space2)
                                                        .fill(FillStyle::solid(canSend ? theme.colorAccent : theme.colorSurfaceDisabled))
                                                        .cornerRadius(theme.radiusFull)
                                                        .cursor(canSend ? Cursor::Hand : Cursor::Arrow)
                                                        .onTap([submit, canSend] {
                                                            if (canSend) {
                                                                submit();
                                                            }
                                                        })),
                }
            ),
        }
            .padding(theme.space4)
            .fill(FillStyle::solid(theme.colorSurfaceOverlay))
            .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
            .cornerRadius(theme.radiusXLarge);
    }
};

struct ChatView : ViewModifiers<ChatView> {
    ChatThread chat;
    std::function<void(std::string const &)> onSend;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        auto draft = useState<std::string>("");

        std::vector<Element> bubbles;
        bubbles.reserve(chat.messages.size());
        for (ChatMessage const &message : chat.messages) {
            bubbles.push_back(ChatBubble {.message = message});
        }

        return VStack {
            .spacing = theme.space4,
            .alignment = Alignment::Center,
            .children = children(
                HStack {
                    .spacing = theme.space2,
                    .alignment = Alignment::Center,
                    .children = children(
                        Text {
                            .text = chat.title,
                            .font = theme.fontHeading,
                            .color = theme.colorTextPrimary,
                            .verticalAlignment = VerticalAlignment::Center
                        }
                            .flex(1.f, 1.f),
                        Text {
                            .text = "Updated " + chat.updatedAt + " ago",
                            .font = theme.fontLabelSmall,
                            .color = theme.colorTextSecondary,
                            .verticalAlignment = VerticalAlignment::Center
                        }
                    ),
                },
                ScrollView {
                    .axis = ScrollAxis::Vertical,
                    .children = children(
                        VStack {
                            .spacing = theme.space3,
                            .children = std::move(bubbles),
                        }
                    ),
                }
                    .flex(1.f, 1.f, 0.f),
                ChatComposer {
                    .value = draft,
                    .onSend = onSend,
                }
            ),
        }
            .padding(theme.space4)
            .fill(FillStyle::solid(theme.colorBackground))
            .flex(1.f, 1.f, 0.f);
    }
};

} // namespace lambda
