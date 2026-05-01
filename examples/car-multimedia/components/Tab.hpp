#pragma once

#include "../Common.hpp"
#include "../Helpers.hpp"

namespace car {

struct Tab : ViewModifiers<Tab> {
    std::string label;
    Reactive::Bindable<bool> active{false};
    std::function<void()> onTap;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        Reactive::Bindable<Color> textColor{[active = active] {
            return active.evaluate() ? Color::primary() : Color::tertiary();
        }};
        Reactive::Bindable<Color> underlineColor{[active = active] {
            return active.evaluate() ? Color::accent() : transparent();
        }};
        return VStack {
            .spacing = theme().space1,
            .alignment = Alignment::Center,
            .children = children(
                Text{.text = label, .font = Font{.size = 12.f, .weight = 500.f}, .color = textColor},
                Rectangle{}.height(2.f).width(42.f).fill(underlineColor).cornerRadius(1.f)
            ),
        }
            .padding(theme().space1, theme().space2, 0.f, theme().space2)
            .cursor(Cursor::Hand)
            .onTap([onTap = onTap] { if (onTap) onTap(); });
    }
};

} // namespace car
