#pragma once

#include "../Common.hpp"

namespace car {

struct Pill : ViewModifiers<Pill> {
    IconName icon = IconName::Info;
    Reactive::Bindable<std::string> label{std::string{}};

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        return HStack {
            .spacing = theme().space2,
            .alignment = Alignment::Center,
            .children = children(
                Icon{.name = icon, .size = 14.f, .color = Color::tertiary()},
                Text{.text = label, .font = Font::subheadline(), .color = Color::secondary()}
            ),
        }
            .padding(6.f, 12.f, 6.f, 12.f)
            .stroke(Color::separator(), 1.f)
            .cornerRadius(theme().radiusFull);
    }
};

} // namespace car
