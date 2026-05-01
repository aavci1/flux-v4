#pragma once

#include "../Common.hpp"

namespace car {

struct FanControl : ViewModifiers<FanControl> {
    Reactive::Bindable<int> value{4};
    int max = 6;
    std::function<void(int)> onChange;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();

        std::vector<Element> bars;
        bars.reserve(static_cast<std::size_t>(max));
        for (int i = 1; i <= max; ++i) {
            Reactive::Bindable<Color> fill{[value = value, i] {
                return i <= value.evaluate() ? Color::accent() : Color::opaqueSeparator();
            }};

            bars.push_back(Rectangle{}.size(4.f, 12.f + static_cast<float>(i))
                               .fill(fill)
                               .cornerRadius(2.f)
                               .cursor(Cursor::Hand)
                               .onTap([onChange = onChange, i] { if (onChange) onChange(i); }));
        }

        return HStack {
            .spacing = theme().space2,
            .alignment = Alignment::Center,
            .children = children(
                Icon{.name = IconName::ModeFan, .size = 16.f, .color = Color::secondary()},
                HStack{.spacing = 2.f, .alignment = Alignment::End, .children = std::move(bars)},
                Text{.text = Reactive::Bindable<std::string>{[value = value] { return std::to_string(value.evaluate()); }},
                     .font = Font::footnote(), .color = Color::tertiary()}
            ),
        }
            .height(36.f)
            .padding(0.f, 4.f, 0.f, 10.f)
            .stroke(Color::separator(), 1.f)
            .cornerRadius(theme().radiusFull);
    }
};

} // namespace car
