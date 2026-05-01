#pragma once

#include "../Common.hpp"

namespace car {

struct AccentIconButton : ViewModifiers<AccentIconButton> {
    Reactive::Bindable<IconName> icon{IconName::PlayArrow};
    float sizeP = 44.f;
    std::function<void()> onTap;

    auto body() const {
        return ZStack {
            .horizontalAlignment = Alignment::Center,
            .verticalAlignment = Alignment::Center,
            .children = children(
                Rectangle{}.size(sizeP, sizeP).fill(Color::accent()).cornerRadius(sizeP * 0.5f),
                Icon{.name = icon, .size = sizeP * 0.5f, .weight = 600.f, .color = Color::accentForeground()}
            ),
        }
            .cursor(Cursor::Hand)
            .onTap([onTap = onTap] { if (onTap) onTap(); });
    }
};

} // namespace car
