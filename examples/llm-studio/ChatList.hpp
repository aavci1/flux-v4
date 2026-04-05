#pragma once

#include <Flux.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include "ChatListRow.hpp"
#include "Divider.hpp"

using namespace flux;

struct ChatList : ViewModifiers<ChatList> {
    const std::vector<Chat> chats = {};
    size_t selectedIndex = 0;

    std::function<void(size_t)> onChatSelected;
    std::function<void()> onNewChat;

    auto body() const {
        Theme const& theme = useEnvironment<Theme>();

        auto ocs = onChatSelected;
        auto elements = std::vector<Element>{};
        for (size_t index = 0; index < chats.size(); index++) {
            size_t i = index;
            elements.push_back(
                Element {
                    ChatListRow {
                        .index = i,
                        .title = chats[i].title,
                        .selected = (i == selectedIndex),
                        .onSelect = ocs,
                    }
                }
            );
        }

        auto oc = onNewChat;

        return VStack {
            .spacing = 8.f,
            .children = children(
                HStack {
                    .alignment = Alignment::Center,
                    .children = children(
                        Text {
                            .text = "Chats",
                            .style = theme.typeTitle,
                            .color = theme.colorTextPrimary
                        },
                        Spacer {},
                        Icon {
                            .name = IconName::Add,
                            .size = theme.typeTitle.size,
                            .color = theme.colorTextPrimary
                        }.cursor(Cursor::Hand)
                        .onTap([oc]() {
                            if (oc) {
                                oc();
                            }
                        })
                    )
                }.padding(16.f, 8.f, 8.f, 8.f),
                Divider {},
                VStack {
                    .spacing = 0.f,
                    .children = elements
                }
            )
        }.width(240.f);
    }
};