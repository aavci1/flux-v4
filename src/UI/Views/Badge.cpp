#include <Flux/UI/Views/Badge.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Views/Text.hpp>

namespace flux {

namespace {

Badge::Style resolveStyle(Badge::Style const &style, Theme const &theme) {
    return Badge::Style {
        .font = resolveFont(style.font, theme.fontLabelSmall),
        .paddingH = resolveFloat(style.paddingH, theme.space2),
        .paddingV = resolveFloat(style.paddingV, theme.space1),
        .cornerRadius = resolveFloat(style.cornerRadius, theme.radiusFull),
        .foregroundColor = resolveColor(style.foregroundColor, theme.colorTextPrimary),
        .backgroundColor = resolveColor(style.backgroundColor, theme.colorAccentSubtle),
    };
}

} // namespace

Element Badge::body() const {
    Badge::Style const resolved = resolveStyle(style, flux::useEnvironment<Theme>());
    return Text {
        .text = label,
        .font = resolved.font,
        .color = resolved.foregroundColor,
        .horizontalAlignment = HorizontalAlignment::Center,
    }
        .padding(resolved.paddingV, resolved.paddingH, resolved.paddingV, resolved.paddingH)
        .fill(FillStyle::solid(resolved.backgroundColor))
        .cornerRadius(resolved.cornerRadius);
}

} // namespace flux
