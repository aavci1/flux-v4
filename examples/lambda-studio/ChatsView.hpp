#pragma once

#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <vector>

#include "AppState.hpp"
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
                            .text = (chat.modelName.empty() ? "" : chat.modelName + "  •  ") + shortenForPreview(chatPreview(chat)),
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
    AppState state;
    std::function<void()> onNewChat;
    std::function<void(int)> onSelectChat;
    std::function<void(int, std::string const &, std::string const &)> onSelectModel;
    std::function<void(int, std::string const &)> onSend;
    std::function<void(int)> onStop;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        std::vector<ChatThread> const &chatThreads = state.chats;

        int const selectedIndex = clampedChatIndex(state);

        std::vector<Element> rows;
        rows.reserve(chatThreads.size());

        for (std::size_t i = 0; i < chatThreads.size(); ++i) {
            rows.push_back(ChatListRow {
                .chat = chatThreads[i],
                .selected = static_cast<int>(i) == selectedIndex,
                .onTap = [onSelectChat = onSelectChat, i] {
                    if (onSelectChat) {
                        onSelectChat(static_cast<int>(i));
                    }
                },
            });
        }

        Element detail = VStack {
            .alignment = Alignment::Center,
            .children = children(
                Text {
                    .text = "No chats yet",
                    .font = theme.fontHeading,
                    .color = theme.colorTextPrimary,
                },
                Text {
                    .text = "Create a new chat or select one from the list.",
                    .font = theme.fontBodySmall,
                    .color = theme.colorTextSecondary,
                    .horizontalAlignment = HorizontalAlignment::Center,
                }
            ),
        }
                             .flex(1.f, 1.f);

        if (selectedIndex >= 0) {
            ChatThread const &selectedChat = chatThreads[static_cast<std::size_t>(selectedIndex)];
            detail = ChatView {
                .chat = selectedChat,
                .localModels = state.localModels,
                .loadedModelPath = state.loadedModelPath,
                .modelLoading = state.modelLoading,
                .onSend = [onSend = onSend, selectedIndex](std::string const &message) {
                    if (onSend) {
                        onSend(selectedIndex, message);
                    } },
                .onStop = [onStop = onStop, selectedIndex] {
                    if (onStop) {
                        onStop(selectedIndex);
                    } },
                .onSelectModel = [onSelectModel = onSelectModel, selectedIndex](std::string const &path, std::string const &name) {
                    if (onSelectModel) {
                        onSelectModel(selectedIndex, path, name);
                    } },
            }
                         .flex(1.f, 1.f);
        }

        return HStack {
            .spacing = 0.f,
            .alignment = Alignment::Stretch,
            .children = children(
                VStack {
                    .spacing = 0.f,
                    .alignment = Alignment::Start,
                    .children = children(
                        HStack {
                            .spacing = theme.space2,
                            .alignment = Alignment::Center,
                            .children = children(
                                Text {
                                    .text = "Chats",
                                    .font = theme.fontHeading,
                                    .color = theme.colorTextPrimary,
                                    .horizontalAlignment = HorizontalAlignment::Leading,
                                }
                                    .flex(1.f, 1.f),
                                IconActionButton {
                                    .icon = IconName::Add,
                                    .onTap = onNewChat,
                                }
                            )
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
                std::move(detail)
            ),
        };
    }
};

} // namespace lambda
