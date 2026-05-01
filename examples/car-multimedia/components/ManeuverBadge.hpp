#pragma once

#include "../Common.hpp"

namespace car {

struct ManeuverBadge : ViewModifiers<ManeuverBadge> {
    IconName icon = IconName::TurnSlightRight;
    float sizeP = 36.f;
    auto body() const {
        return ZStack {
            .horizontalAlignment = Alignment::Center,
            .verticalAlignment = Alignment::Center,
            .children = children(
                Rectangle{}.size(sizeP, sizeP).fill(Color::accent()).cornerRadius(8.f),
                Icon{.name = icon, .size = sizeP * 0.58f, .weight = 600.f, .color = Color::accentForeground()}
            ),
        };
    }
};

} // namespace car
