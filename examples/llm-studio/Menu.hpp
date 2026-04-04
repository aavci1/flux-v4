#pragma once

#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

using namespace flux;

struct MenuItem: ViewModifiers<MenuItem> {
    IconName icon;
    std::string label;

    auto body() const {
        Theme const& theme = useEnvironment<Theme>();

        auto isHovered = useHover();
        auto isPressed = usePress();

        return HStack {
            .spacing = 12.f,
            .alignment = Alignment::Center,
            .children = children(
                Icon {
                    .name = icon,
                    .size = theme.typeSubtitle.size,
                    .weight = 200.f,
                    .color = theme.colorTextPrimary,
                },
                Text {
                    .text = label,
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
        .fill(isHovered ? FillStyle::solid(Color::hex(0xEBEDEF)) : FillStyle::none())
        // .shadow(isHovered ? ShadowStyle {
        //     .radius = 1.f,
        //     .offset = {0.f, 0.f},
        //     .color = Color::hex(0xC0C0C0)
        // } : ShadowStyle::none())
        // .cornerRadius(8.f)
        .cursor(Cursor::Hand)
        .padding(8.f, 16.f, 8.f, 16.f);
    }
};

struct Menu: ViewModifiers<Menu> {
    auto body() const {
        Theme const& theme = useEnvironment<Theme>();

        return VStack {
            .spacing = 0.f,
            .children = children(
                MenuItem {
                    .icon = IconName::Home,
                    .label = "Home"
                },
                MenuItem {
                    .icon = IconName::Subtitles,
                    .label = "Chats"
                },
                MenuItem {
                    .icon = IconName::Robot,
                    .label = "Models"
                },
                MenuItem {
                    .icon = IconName::Settings,
                    .label = "Settings"
                }
            )
        };
    }
};

