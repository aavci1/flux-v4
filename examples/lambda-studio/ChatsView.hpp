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
#include "SharedViews.hpp"
using namespace flux;

namespace lambda {

struct ChatListRow : ViewModifiers<ChatListRow> {
    ChatThread chat;
    bool selected = false;
    std::function<void()> onTap;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        Color const titleColor = theme.colorTextPrimary;
        Color const detailColor = selected ? theme.colorTextPrimary : theme.colorTextSecondary;

        return ListRow {
            .content = HStack {
                .spacing = theme.space1,
                .alignment = Alignment::Start,
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
                                .text = chatPreview(chat),
                                .font = theme.fontBodySmall,
                                .color = detailColor,
                                .horizontalAlignment = HorizontalAlignment::Leading,
                                .wrapping = TextWrapping::Wrap,
                                .maxLines = 2,
                            }
                        ),
                    }
                        .flex(1.f, 1.f),
                    Text {
                        .text = chatUpdatedAtLabel(chat),
                        .font = theme.fontLabelSmall,
                        .color = detailColor,
                        .horizontalAlignment = HorizontalAlignment::Trailing,
                        .verticalAlignment = VerticalAlignment::Center,
                    }
                ),
            },
            .selected = selected,
            .onTap = onTap,
        };
    }
};

struct ChatsView : ViewModifiers<ChatsView> {
    AppState state;
    std::function<void()> onNewChat;
    std::function<void(int)> onSelectChat;
    std::function<void(int, std::string const &, std::string const &)> onSelectModel;
    std::function<void(int, std::string const &)> onSend;
    std::function<void(int)> onStop;
    std::function<void(int)> onDeleteChat;
    std::function<void(int, int)> onToggleReasoning;
    std::function<void(int, int)> onDeleteMessage;

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

        Element detail = Element {EmptyStatePanel {
            .title = "No Chats",
            .detail = "Create a new chat or select one from the list.",
        }}
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
                .onDeleteChat = [onDeleteChat = onDeleteChat, selectedIndex] {
                    if (onDeleteChat) {
                        onDeleteChat(selectedIndex);
                    } },
                .onToggleReasoning = [onToggleReasoning = onToggleReasoning, selectedIndex](int messageIndex) {
                    if (onToggleReasoning) {
                        onToggleReasoning(selectedIndex, messageIndex);
                    } },
                .onDeleteMessage = [onDeleteMessage = onDeleteMessage, selectedIndex](int messageIndex) {
                    if (onDeleteMessage) {
                        onDeleteMessage(selectedIndex, messageIndex);
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
            .children = {
                VStack {
                    .spacing = 0.f,
                    .alignment = Alignment::Start,
                    .children = {
                        HStack {
                            .spacing = theme.space2,
                            .alignment = Alignment::Center,
                            .children = {
                                Text {
                                    .text = "Chats",
                                    .font = theme.fontHeading,
                                    .color = theme.colorTextPrimary,
                                    .horizontalAlignment = HorizontalAlignment::Leading,
                                }
                                    .flex(1.f, 1.f),
                                IconButton {
                                    .icon = IconName::Add,
                                    .style = {
                                        .size = theme.fontHeading.size,
                                        .weight = theme.fontLabel.weight,
                                    },
                                    .onTap = onNewChat
                                }
                            }
                        }.padding(theme.space4),
                        Rectangle {}.size(0.f, 1.f).fill(FillStyle::solid(theme.colorBorderSubtle)),
                        ListView {
                            .rows = std::move(rows),
                        }
                            .flex(1.f, 1.f, 0.f),
                    },
                }
                    .fill(FillStyle::solid(theme.colorSurfaceOverlay))
                    .size(320.f, 0.f),
                Rectangle {}.size(1.f, 0.f).fill(FillStyle::solid(theme.colorBorderSubtle)),
                std::move(detail),
            },
        };
    }
};

} // namespace lambda
