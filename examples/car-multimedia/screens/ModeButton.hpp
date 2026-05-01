#pragma once

#include "../Common.hpp"

namespace car {

struct ModeButton : ViewModifiers<ModeButton> {
    std::string label;
    IconName icon = IconName::Weekend;
    Reactive::Bindable<bool> active{false};
    std::function<void()> onTap;
    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        Reactive::Bindable<Color> bg{[active = active] { return active.evaluate() ? Color::selectedContentBackground() : Color::controlBackground(); }};
        Reactive::Bindable<Color> border{[active = active] { return active.evaluate() ? Color::accent() : Color::separator(); }};
        Reactive::Bindable<Color> fg{[active = active] { return active.evaluate() ? Color::accent() : Color::primary(); }};
        return VStack {
            .spacing = 6.f,
            .alignment = Alignment::Start,
            .children = children(Icon{.name = icon, .size = 20.f, .weight = 500.f, .color = fg}, Text{.text = label, .font = Font{.size = 13.f, .weight = 500.f}, .color = fg}),
        }.padding(14.f, 12.f, 14.f, 12.f).fill(bg).stroke(border, 1.f).cornerRadius(theme().radiusLarge).cursor(Cursor::Hand)
            .onTap([onTap = onTap] { if (onTap) onTap(); });
    }
};

} // namespace car
