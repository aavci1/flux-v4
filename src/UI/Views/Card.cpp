#include <Flux/UI/Views/Card.hpp>
#include <Flux/UI/Hooks.hpp>

namespace flux {

namespace {

Card::Style resolveStyle(Card::Style const &style, Theme const &theme) {
  float const padding = resolveFloat(style.padding, theme.space5);
  return Card::Style {
      .padding = padding,
      .paddingH = resolveFloat(style.paddingH, padding),
      .paddingV = resolveFloat(style.paddingV, padding),
      .cornerRadius = resolveFloat(style.cornerRadius, theme.radiusXLarge),
      .borderWidth = resolveFloat(style.borderWidth, 1.f),
      .backgroundColor = resolveColor(style.backgroundColor, theme.elevatedBackgroundColor, theme),
      .borderColor = resolveColor(style.borderColor, theme.separatorColor, theme),
      .shadow = style.shadow,
  };
}

} // namespace

Element Card::body() const {
  Card::Style const resolved = resolveStyle(style, useEnvironment<Theme>());

  StrokeStyle const stroke =
      resolved.borderWidth <= 0.f || resolved.borderColor.a <= 0.001f
          ? StrokeStyle::none()
          : StrokeStyle::solid(resolved.borderColor, resolved.borderWidth);

  Element content = child;
  return std::move(content)
      .padding(resolved.paddingV, resolved.paddingH, resolved.paddingV, resolved.paddingH)
      .fill(FillStyle::solid(resolved.backgroundColor))
      .stroke(stroke)
      .cornerRadius(CornerRadius {resolved.cornerRadius})
      .shadow(resolved.shadow);
}

} // namespace flux
