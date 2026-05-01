#pragma once

#include "../Common.hpp"

namespace car {

namespace detail {
inline int hashToHue(std::string const& s) {
    std::uint32_t h = 0;
    for (char c : s) h = h * 31u + static_cast<std::uint8_t>(c);
    return static_cast<int>(h % 360u);
}

inline Color hslToColor(int hue, float sat, float light, float alpha = 1.f) {
    float const h = static_cast<float>(((hue % 360) + 360) % 360) / 60.f;
    float const c = (1.f - std::fabs(2.f * light - 1.f)) * sat;
    float const x = c * (1.f - std::fabs(std::fmod(h, 2.f) - 1.f));
    float r = 0.f;
    float g = 0.f;
    float b = 0.f;
    if (h < 1.f) { r = c; g = x; }
    else if (h < 2.f) { r = x; g = c; }
    else if (h < 3.f) { g = c; b = x; }
    else if (h < 4.f) { g = x; b = c; }
    else if (h < 5.f) { r = x; b = c; }
    else { r = c; b = x; }
    float const m = light - c * 0.5f;
    return Color{r + m, g + m, b + m, alpha};
}
} // namespace detail

struct AlbumArt : ViewModifiers<AlbumArt> {
    Reactive::Bindable<std::string> seed{std::string{}};
    float sizeP = 64.f;

    auto body() const {
        Reactive::Bindable<FillStyle> fill{[seed = seed] {
            int const hue = detail::hashToHue(seed.evaluate());
            Color const base = detail::hslToColor(hue, 0.35f, 0.28f);
            Color const deeper = detail::hslToColor(hue + 40, 0.25f, 0.18f);
            return FillStyle::linearGradient(base, deeper, Point{0.f, 0.f}, Point{1.f, 1.f});
        }};
        return ZStack {
            .horizontalAlignment = Alignment::Center,
            .verticalAlignment = Alignment::Center,
            .children = children(
                Rectangle{}.size(sizeP, sizeP).fill(fill).cornerRadius(8.f),
                Text{.text = "♪", .font = Font{.size = sizeP * 0.32f, .weight = 700.f}, .color = Color{1.f, 1.f, 1.f, 0.18f}}
                    .translate(sizeP * 0.32f, sizeP * 0.32f)
            ),
        }.stroke(Color::separator(), 1.f).cornerRadius(8.f);
    }
};

} // namespace car
