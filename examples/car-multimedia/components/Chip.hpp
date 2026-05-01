#pragma once

#include "../Common.hpp"
#include "../Helpers.hpp"

namespace car {

struct Chip : ViewModifiers<Chip> {
    IconName icon = IconName::Info;
    std::string label;
    Reactive::Bindable<bool> active{false};
    std::function<void()> onTap;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        auto hovered = useState(false);
        Reactive::Bindable<Color> bg{[hovered, active = active] {
            if (active.evaluate()) return Color::selectedContentBackground();
            return hovered() ? Color::selectedContentBackground() : transparent();
        }};
        Reactive::Bindable<Color> fg{[active = active] {
            return active.evaluate() ? Color::accent() : Color::secondary();
        }};
        Reactive::Bindable<Color> border{[active = active] {
            return active.evaluate() ? Color::accent() : Color::separator();
        }};
        return HStack {
            .spacing = 6.f,
            .alignment = Alignment::Center,
            .children = children(
                Icon{.name = icon, .size = 16.f, .weight = 500.f, .color = fg},
                Text{.text = label, .font = Font{.size = 12.f, .weight = 500.f}, .color = fg}
            ),
        }
            .height(36.f)
            .padding(0.f, 12.f, 0.f, 12.f)
            .fill(bg)
            .stroke(border, 1.f)
            .cornerRadius(theme().radiusFull)
            .cursor(Cursor::Hand)
            .onPointerEnter(std::function<void()>{[hovered] { hovered = true; }})
            .onPointerExit(std::function<void()>{[hovered] { hovered = false; }})
            .onTap([onTap = onTap] { if (onTap) onTap(); });
    }
};

} // namespace car
