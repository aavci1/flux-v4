#include <Flux/UI/Views/ProgressBar.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>

namespace flux {

namespace {

ProgressBar::Style resolveStyle(ProgressBar::Style const &style, Theme const &theme) {
    float const height = std::max(1.f, resolveFloat(style.height, 6.f));
    return ProgressBar::Style {
        .width = std::max(1.f, resolveFloat(style.width, 112.f)),
        .height = height,
        .cornerRadius = resolveFloat(style.cornerRadius, height * 0.5f),
        .trackColor = resolveColor(style.trackColor, theme.colorSurfaceHover),
        .fillColor = resolveColor(style.fillColor, theme.colorAccent),
    };
}

} // namespace

Element ProgressBar::body() const {
    ProgressBar::Style const resolved = resolveStyle(style, flux::useEnvironment<Theme>());
    float const clamped = std::clamp(progress, 0.f, 1.f);

    return Element {ZStack {
        .horizontalAlignment = Alignment::Start,
        .verticalAlignment = Alignment::Start,
        .children = children(
            Rectangle {}
                .fill(FillStyle::solid(resolved.trackColor))
                .size(resolved.width, resolved.height)
                .cornerRadius(resolved.cornerRadius),
            Rectangle {}
                .fill(FillStyle::solid(resolved.fillColor))
                .size(resolved.width * clamped, resolved.height)
                .cornerRadius(resolved.cornerRadius)
        ),
    }}
        .size(resolved.width, resolved.height);
}

} // namespace flux
