#pragma once

#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <functional>
#include <optional>
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
        auto theme = useEnvironment<Theme>();

        auto hovered = useState(false);
        auto pressed = useState(false);

        Bindable<Color> color {[hovered, pressed, selected = selected] {
            if (pressed() || selected) {
                return Color::accent();
            }
            if (hovered()) {
                return Color::selectedContentBackground();
            }
            return Color::secondary();
        }};

        return VStack {
            .spacing = theme().space1,
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
                    .font = Font::caption(),
                    .color = color,
                    .horizontalAlignment = HorizontalAlignment::Center,
                    .verticalAlignment = VerticalAlignment::Center,
                }
            ),
        }
            .cursor(Cursor::Hand)
            .focusable(true)
            .onPointerEnter(std::function<void()> {[hovered] { hovered = true; }})
            .onPointerExit(std::function<void()> {[hovered, pressed] {
                hovered = false;
                pressed = false;
            }})
            .onPointerDown(std::function<void(Point)> {[pressed](Point) { pressed = true; }})
            .onPointerUp(std::function<void(Point)> {[pressed](Point) { pressed = false; }})
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
        auto theme = useEnvironment<Theme>();

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

        std::vector<Element> topChildren;
        std::optional<Module> bottomModule;
        for (Module const &module : modules) {
            if (module.title == "Settings") {
                bottomModule = module;
                continue;
            }
            topChildren.push_back(makeButton(module.icon, module.title));
        }
        topChildren.push_back(Spacer {});
        if (bottomModule.has_value()) {
            topChildren.push_back(makeButton(bottomModule->icon, bottomModule->title));
        }

        return VStack {
            .spacing = theme().space6,
            .alignment = Alignment::Center,
            .children = std::move(topChildren),
        }
            .padding(theme().space4, theme().space2, theme().space4, theme().space2);
    }
};

} // namespace lambda
