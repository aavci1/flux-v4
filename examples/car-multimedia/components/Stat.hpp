#pragma once

#include "../Common.hpp"

namespace car {

struct Stat : ViewModifiers<Stat> {
    std::string label;
    Reactive::Bindable<std::string> value{std::string{}};

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        return VStack {
            .spacing = theme().space1,
            .alignment = Alignment::Start,
            .children = children(
                Text{.text = label, .font = Font::caption2(), .color = Color::tertiary()},
                Text{.text = value, .font = Font{.size = 13.f, .weight = 500.f}, .color = Color::primary()}
            ),
        };
    }
};

} // namespace car
