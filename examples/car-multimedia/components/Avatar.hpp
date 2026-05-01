#pragma once

#include "../Common.hpp"

namespace car {

struct Avatar : ViewModifiers<Avatar> {
    Reactive::Bindable<std::string> initials{std::string{}};
    float sizeP = 32.f;
    Reactive::Bindable<bool> useAccent{false};

    auto body() const {
        Reactive::Bindable<FillStyle> bg{[useAccent = useAccent] {
            if (!useAccent.evaluate()) {
                return FillStyle::solid(Color::controlBackground());
            }
            return FillStyle::linearGradient(Color::accent(),
                                             Color{0.04f, 0.52f, 1.f, 0.8f},
                                             Point{0.f, 0.f},
                                             Point{1.f, 1.f});
        }};
        Reactive::Bindable<Color> fg{[useAccent = useAccent] {
            return useAccent.evaluate() ? Color::accentForeground() : Color::secondary();
        }};
        return ZStack {
            .horizontalAlignment = Alignment::Center,
            .verticalAlignment = Alignment::Center,
            .children = children(
                Rectangle{}.size(sizeP, sizeP).fill(bg).cornerRadius(sizeP * 0.5f),
                Text {
                    .text = initials,
                    .font = Font{.size = sizeP * 0.34f, .weight = 600.f},
                    .color = fg,
                    .horizontalAlignment = HorizontalAlignment::Center,
                    .verticalAlignment = VerticalAlignment::Center,
                }
            ),
        };
    }
};

} // namespace car
