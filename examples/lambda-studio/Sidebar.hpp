#pragma once

#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <functional>
#include <string>

using namespace flux;

namespace lambda {

struct Module {
    IconName icon;
    std::string title;
};

struct SidebarButton : ViewModifiers<SidebarButton> {
    IconName icon;
    std::string label;
    bool selected = false;
    std::function<void()> onTap;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        bool const hovered = useHover();
        bool const pressed = usePress();
        bool const focused = useFocus();

        Color color = theme.colorTextSecondary;

        if (hovered) {
            color = theme.colorTextPrimary;
        }

        if (pressed || selected) {
            color = theme.colorAccent;
        }

        return VStack {
            .spacing = theme.space1,
            .alignment = Alignment::Center,
            .children = children(
                Icon {
                    .name = icon,
                    .size = 24.f,
                    .weight = 600.f,
                    .color = color,
                },
                Text {
                    .text = label,
                    .font = theme.fontLabelSmall,
                    .color = color,
                    .horizontalAlignment = HorizontalAlignment::Center,
                    .verticalAlignment = VerticalAlignment::Center,
                }
            ),
        }
            .cursor(Cursor::Hand)
            .focusable(true)
            .onTap([onTap = onTap] {
                if (onTap) {
                    onTap();
                }
            });
    }
};

struct Sidebar : ViewModifiers<Sidebar> {
    std::vector<Module> modules = {};
    std::string selectedTitle = "Chats";
    std::function<void(std::string)> onSelect;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        auto makeButton = [this](IconName icon, std::string title) {
            return SidebarButton {
                .icon = icon,
                .label = title,
                .selected = title == selectedTitle,
                .onTap = [title, onSelect = onSelect] {
                    if (onSelect) {
                        onSelect(title);
                    }
                },
            };
        };

        return VStack {
            .spacing = theme.space6,
            .alignment = Alignment::Center,
            .children = children(
                makeButton(IconName::ChatBubble, "Chats"),
                makeButton(IconName::ModelTraining, "Models"),
                Spacer {},
                makeButton(IconName::Settings, "Settings")
            ),
        }
            .padding(theme.space4)
            .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f));
    }
};

} // namespace lambda
