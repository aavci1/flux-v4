#pragma once

#include <Flux.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include "Divider.hpp"
#include "Types.hpp"

using namespace flux;


struct ChatItem : ViewModifiers<ChatItem> {
    size_t index = 0;
    std::string title;
    bool selected = false;
    std::function<void(size_t)> onSelect;

    auto body() const {
        Theme const& theme = useEnvironment<Theme>();

        auto isHovered = useHover();

        FillStyle const rowFill = selected
            ? FillStyle::solid(Color::hex(0xDCE8F5))
            : isHovered ? FillStyle::solid(Color::hex(0xEBEDEF)) : FillStyle::none();

        return HStack {
            .spacing = 12.f,
            .alignment = Alignment::Center,
            .children = children(
                Text {
                    .text = title,
                    .style = theme.typeSubtitle,
                    .color = theme.colorTextPrimary,
                    .verticalAlignment = VerticalAlignment::Center,
                    .wrapping = TextWrapping::NoWrap,
                },
                Spacer {},
                Icon {
                    .name = IconName::MoreHoriz,
                    .size = theme.typeSubtitle.size,
                    .weight = 300.f,
                    .color = isHovered ? theme.colorTextPrimary : Colors::transparent,
                }
            )
        }
        .fill(rowFill)
        .cursor(Cursor::Hand)
        .padding(8.f)
        .onTap([onSelect = onSelect, idx = index]() {
            if (onSelect) {
                onSelect(idx);
            }
        });
    }
};


struct ChatsPanel : ViewModifiers<ChatsPanel> {
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
                    ChatItem {
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