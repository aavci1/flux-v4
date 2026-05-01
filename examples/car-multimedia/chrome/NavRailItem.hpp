#pragma once

#include "../Common.hpp"
#include "../Helpers.hpp"

namespace car {

struct NavRailItem : ViewModifiers<NavRailItem> {
    IconName icon = IconName::Apps;
    std::string label;
    std::function<bool()> active;
    std::function<void()> onTap;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        auto hovered = useState(false);
        auto pressed = useState(false);
        Reactive::Bindable<bool> isActive{[active = active] { return active && active(); }};
        Reactive::Bindable<Color> tint{[hovered, pressed, isActive] {
            if (isActive.evaluate()) return Color::accent();
            if (pressed()) return Color::accent();
            return hovered() ? Color::primary() : Color::secondary();
        }};
        Reactive::Bindable<Color> bg{[hovered, isActive] {
            if (isActive.evaluate()) return Color::selectedContentBackground();
            return hovered() ? Color::selectedContentBackground() : transparent();
        }};
        return VStack {
            .spacing = theme().space1,
            .alignment = Alignment::Center,
            .children = children(
                Icon{.name = icon, .size = 24.f, .weight = 500.f, .color = tint},
                Text{.text = label, .font = Font::caption2(), .color = tint, .horizontalAlignment = HorizontalAlignment::Center}
            ),
        }
            .padding(theme().space3, theme().space1, theme().space3, theme().space1)
            .fill(bg)
            .cornerRadius(theme().radiusMedium)
            .cursor(Cursor::Hand)
            .focusable(true)
            .onPointerEnter(std::function<void()>{[hovered] { hovered = true; }})
            .onPointerExit(std::function<void()>{[hovered, pressed] { hovered = false; pressed = false; }})
            .onPointerDown(std::function<void(Point)>{[pressed](Point) { pressed = true; }})
            .onPointerUp(std::function<void(Point)>{[pressed](Point) { pressed = false; }})
            .onTap([onTap = onTap] { if (onTap) onTap(); });
    }
};

} // namespace car
