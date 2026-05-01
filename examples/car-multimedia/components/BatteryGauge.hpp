#pragma once

#include "../Common.hpp"

namespace car {

struct BatteryGauge : ViewModifiers<BatteryGauge> {
    Reactive::Bindable<int> level{72};

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        std::vector<Element> bars;
        bars.reserve(20);
        for (int i = 0; i < 20; ++i) {
            Reactive::Bindable<Color> fill{[level = level, i] {
                return i < level.evaluate() / 5 ? Color::accent() : Color::separator();
            }};
            bars.push_back(Rectangle{}.height(6.f).fill(fill).cornerRadius(1.f).flex(1.f, 1.f, 0.f));
        }
        return VStack {
            .spacing = theme().space2,
            .alignment = Alignment::Stretch,
            .children = children(
                HStack {
                    .spacing = theme().space2,
                    .alignment = Alignment::Center,
                    .children = children(
                        Icon{.name = IconName::BatteryChargingFull, .size = 14.f, .weight = 600.f, .color = Color::accent()},
                        Text{.text = Reactive::Bindable<std::string>{[level = level] { return std::to_string(level.evaluate()) + "%"; }},
                             .font = Font{.size = 12.f, .weight = 500.f}, .color = Color::secondary()},
                        Spacer{}.flex(1.f, 1.f),
                        Text{.text = "Charging / 7.2 kW", .font = Font::caption2(), .color = Color::tertiary()}
                    ),
                },
                HStack{.spacing = 2.f, .alignment = Alignment::Stretch, .children = std::move(bars)}.height(6.f)
            ),
        };
    }
};

} // namespace car
