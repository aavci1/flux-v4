#pragma once

#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <cstddef>
#include <vector>

#include "ChatModels.hpp"
#include "ChatView.hpp"
#include "Divider.hpp"

using namespace flux;

namespace lambda {

struct ChatListRow : ViewModifiers<ChatListRow> {
    ChatThread chat;
    bool selected = false;
    std::function<void()> onTap;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        bool const hovered = useHover();
        bool const pressed = usePress();

        Color const fill = selected ? theme.colorAccentSubtle : pressed ? theme.colorSurfaceRowHover :
                                                            hovered     ? theme.colorSurfaceHover :
                                                                          Colors::transparent;
        Color const titleColor = theme.colorTextPrimary;
        Color const detailColor = selected ? theme.colorTextPrimary : theme.colorTextSecondary;

        return HStack {
            .spacing = theme.space3,
            .alignment = Alignment::Center,
            .children = children(
                VStack {
                    .alignment = Alignment::Stretch,
                    .children = children(
                        Text {
                            .text = chat.title,
                            .font = theme.fontLabel,
                            .color = titleColor,
                            .horizontalAlignment = HorizontalAlignment::Leading,
                            .wrapping = TextWrapping::NoWrap,
                        },
                        Text {
                            .text = shortenForPreview(chatPreview(chat)),
                            .font = theme.fontBodySmall,
                            .color = detailColor,
                            .horizontalAlignment = HorizontalAlignment::Leading,
                            .wrapping = TextWrapping::Wrap,
                        }
                    ),
                }
                    .flex(1.f, 1.f),
                Text {
                    .text = chat.updatedAt,
                    .font = theme.fontLabelSmall,
                    .color = detailColor,
                    .horizontalAlignment = HorizontalAlignment::Trailing,
                    .verticalAlignment = VerticalAlignment::Center,
                }
            ),
        }
            .padding(theme.space4)
            .fill(FillStyle::solid(fill))
            // .cornerRadius(theme.radiusLarge)
            .cursor(Cursor::Hand)
            .focusable(true)
            .onTap([onTap = onTap] {
                if (onTap) {
                    onTap();
                }
            });
    }
};

struct ChatsView : ViewModifiers<ChatsView> {
    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        auto selectedChatIndex = useState<int>(0);
        auto chats = useState<std::vector<ChatThread>>(sampleChatThreads());
        std::vector<ChatThread> const chatThreads = *chats;

        std::size_t const selectedIndex =
            static_cast<std::size_t>(std::max(0, std::min(*selectedChatIndex, static_cast<int>(chatThreads.size() - 1))));
        ChatThread const &selectedChat = chatThreads[selectedIndex];

        std::vector<Element> rows;
        rows.reserve(chatThreads.size());

        for (std::size_t i = 0; i < chatThreads.size(); ++i) {
            rows.push_back(ChatListRow {
                .chat = chatThreads[i],
                .selected = i == selectedIndex,
                .onTap = [selectedChatIndex, i] {
                    selectedChatIndex = static_cast<int>(i);
                },
            });
        }

        return HStack {
            .spacing = 0.f,
            .alignment = Alignment::Stretch,
            .children = children(
                VStack {
                    .spacing = 0.f,
                    .alignment = Alignment::Start,
                    .children = children(
                        Text {
                            .text = "Chats",
                            .font = theme.fontHeading,
                            .color = theme.colorTextPrimary,
                            .horizontalAlignment = HorizontalAlignment::Leading,
                        }
                            .padding(theme.space4),
                        Divider {},
                        ScrollView {
                            .axis = ScrollAxis::Vertical,
                            .children = children(
                                VStack {
                                    .spacing = 0.f,
                                    .children = std::move(rows),
                                }
                            ),
                        }
                            .flex(1.f, 1.f, 0.f)
                    ),
                }
                    // .padding(theme.space4)
                    .fill(FillStyle::solid(theme.colorSurfaceOverlay))
                    // .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
                    .size(320.f, 0.f),
                Divider {
                    .orientation = Divider::Orientation::Vertical,
                },
                ChatView {
                    .chat = selectedChat,
                    .onSend = [chats, selectedIndex](std::string const &message) {
                        if (message.empty()) {
                            return;
                        }

                        auto nextChats = *chats;
                        ChatThread &thread = nextChats[selectedIndex];
                        thread.messages.push_back(ChatMessage {
                            .role = ChatRole::User,
                            .text = message,
                        });
                        thread.messages.push_back(ChatMessage {
                            .role = ChatRole::Assistant,
                            .text = mockAssistantReply(thread.title, message),
                        });
                        thread.updatedAt = "now";

                        chats = std::move(nextChats);
                    },
                }
                    .flex(1.f, 1.f)
            ),
        };
    }
};

} // namespace lambda
