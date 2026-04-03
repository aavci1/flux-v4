#include <Flux/Core/Window.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Button.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <Flux/Core/KeyCodes.hpp>

#include <cmath>
#include <cstdint>

namespace flux {

namespace {

Color lighten(Color c, float t) {
  Color const w = Colors::white;
  return Color{ lerp(c.r, w.r, t), lerp(c.g, w.g, t), lerp(c.b, w.b, t), c.a };
}

Color darken(Color c, float t) {
  Color const b = Colors::black;
  return Color{ lerp(c.r, b.r, t), lerp(c.g, b.g, t), lerp(c.b, b.b, t), c.a };
}

struct ButtonColors {
  Color fill{};
  Color fillHover{};
  Color fillPress{};
  Color label{};
  Color labelHover{};
  Color labelPress{};
  Color border{};
  Color focusRing{};
};

ButtonColors deriveColors(ButtonVariant variant, Color accent, Color destructive, Color onAccent,
                          Color onDanger) {
  switch (variant) {
  case ButtonVariant::Primary:
    return {
        .fill = accent,
        .fillHover = lighten(accent, 0.08f),
        .fillPress = darken(accent, 0.08f),
        .label = onAccent,
        .labelHover = onAccent,
        .labelPress = onAccent,
        .border = Colors::transparent,
        .focusRing = accent,
    };
  case ButtonVariant::Secondary:
    return {
        .fill = Color{ accent.r, accent.g, accent.b, 0.08f },
        .fillHover = Color{ accent.r, accent.g, accent.b, 0.14f },
        .fillPress = Color{ accent.r, accent.g, accent.b, 0.20f },
        .label = accent,
        .labelHover = accent,
        .labelPress = accent,
        .border = Color{ accent.r, accent.g, accent.b, 0.35f },
        .focusRing = accent,
    };
  case ButtonVariant::Destructive:
    return {
        .fill = destructive,
        .fillHover = lighten(destructive, 0.08f),
        .fillPress = darken(destructive, 0.08f),
        .label = onDanger,
        .labelHover = onDanger,
        .labelPress = onDanger,
        .border = Colors::transparent,
        .focusRing = destructive,
    };
  case ButtonVariant::Ghost:
    return {
        .fill = Colors::transparent,
        .fillHover = Color{ accent.r, accent.g, accent.b, 0.08f },
        .fillPress = Color{ accent.r, accent.g, accent.b, 0.14f },
        .label = accent,
        .labelHover = accent,
        .labelPress = accent,
        .border = Colors::transparent,
        .focusRing = accent,
    };
  case ButtonVariant::Link:
    return {
        .fill = Colors::transparent,
        .fillHover = Colors::transparent,
        .fillPress = Colors::transparent,
        .label = accent,
        .labelHover = lighten(accent, 0.12f),
        .labelPress = darken(accent, 0.12f),
        .border = Colors::transparent,
        .focusRing = accent,
    };
  }
  return {};
}

} // namespace

Element Button::body() const {
  Theme const& theme = useEnvironment<Theme>();
  Color const accent = resolveColor(accentColor, theme.colorAccent);
  Color const destructive = resolveColor(destructiveColor, theme.colorDanger);
  Font const fontResolved = resolveFont(font, theme.typeLabel.toFont());

  Transition const trFast =
      theme.reducedMotion ? Transition::instant() : Transition::ease(theme.durationFast);
  Transition const trMed =
      theme.reducedMotion ? Transition::instant() : Transition::ease(theme.durationMedium);

  bool const hovered = useHover();
  bool const pressed = usePress();
  bool const focused = useFocus();
  bool const keyboardFocused = useKeyboardFocus();
  bool const isLink = (variant == ButtonVariant::Link);

  bool effectivelyDisabled = disabled;
  if (!effectivelyDisabled && !actionName.empty()) {
    if (Runtime* rt = Runtime::current()) {
      effectivelyDisabled = !rt->window().isActionEnabled(actionName);
    }
  }

  ButtonColors const colors =
      deriveColors(variant, accent, destructive, theme.colorOnAccent, theme.colorOnDanger);

  auto fillAnim = useAnimated<Color>(colors.fill);
  if (!isLink) {
    Color const fillTarget =
        effectivelyDisabled ? theme.colorSurfaceDisabled
                            : pressed ? colors.fillPress
                            : hovered ? colors.fillHover
                                      : colors.fill;
    if (*fillAnim != fillTarget) {
      fillAnim.set(fillTarget, trMed);
    }
  }

  auto labelAnim = useAnimated<Color>(colors.label);
  {
    Color const labelTarget =
        effectivelyDisabled
            ? theme.colorTextDisabled
            : (isLink ? (pressed ? colors.labelPress : hovered ? colors.labelHover : colors.label) : colors.label);
    if (*labelAnim != labelTarget) {
      labelAnim.set(labelTarget, trMed);
    }
  }

  auto scaleAnim = useAnimated<float>(1.f);
  if (!isLink) {
    float const scaleTarget = (pressed && !effectivelyDisabled) ? 0.97f : 1.f;
    if (std::abs(*scaleAnim - scaleTarget) > 0.001f) {
      scaleAnim.set(scaleTarget, trFast);
    }
  }

  if (!actionName.empty() && onTap) {
    useWindowAction(actionName, [onTap = onTap, effectivelyDisabled] {
      if (!effectivelyDisabled) {
        onTap();
      }
    }, [effectivelyDisabled] { return !effectivelyDisabled; });
  }

  float const h =
      isLink ? 0.f : (height > 0.f ? height : theme.controlHeightMedium);
  float const effPaddingH = isLink ? 0.f : resolveFloat(paddingH, theme.space4);

  auto handleTap = [onTap = onTap, effectivelyDisabled]() {
    if (effectivelyDisabled) {
      return;
    }
    if (onTap) {
      onTap();
    }
  };
  auto handleKey = [handleTap](KeyCode k, Modifiers) {
    if (k == keys::Return || k == keys::Space) {
      handleTap();
    }
  };

  bool const showFocusRing = !effectivelyDisabled && focused &&
                             (isLink ? keyboardFocused : true);
  float const radiusResolved =
      isLink ? theme.radiusXSmall : resolveFloat(cornerRadius, theme.radiusMedium);
  CornerRadius const cr{radiusResolved};

  // Focus is drawn as this rectangle's stroke (inset along the edge), not a separate outset ring: true
  // outside gap would need layout/`resolveLeafBounds` support or a dedicated FocusRing primitive.
  StrokeStyle stroke{};
  if (showFocusRing) {
    stroke = StrokeStyle::solid(colors.focusRing, 2.f);
  } else if (!effectivelyDisabled && colors.border.a > 0.01f) {
    stroke = StrokeStyle::solid(colors.border, 1.f);
  } else {
    stroke = StrokeStyle::none();
  }

  ShadowStyle bgShadow = ShadowStyle::none();
  if (!isLink && variant != ButtonVariant::Ghost && !effectivelyDisabled) {
    bgShadow = ShadowStyle{.radius = theme.shadowRadiusControl,
                           .offset = {0.f, theme.shadowOffsetYControl},
                           .color = theme.shadowColor};
  }

  Element labelEl =
      Text{
          .text = label,
          .style = TextStyle::fromFont(fontResolved),
          .color = *labelAnim,
          .horizontalAlignment = isLink ? HorizontalAlignment::Leading : HorizontalAlignment::Center,
          .verticalAlignment = VerticalAlignment::Center,
      }
          .padding(effPaddingH);

  auto content = ZStack{
      .hAlign = HorizontalAlignment::Center,
      .vAlign = VerticalAlignment::Center,
      .children =
          children(
              Rectangle{
                  .fill = FillStyle::solid(*fillAnim),
                  .stroke = stroke,
                  .shadow = bgShadow,
              }
                  .height(h)
                  .cursor(effectivelyDisabled ? Cursor::Inherit : Cursor::Hand)
                  .focusable(!effectivelyDisabled)
                  .onKeyDown(effectivelyDisabled ? std::function<void(KeyCode, Modifiers)>{}
                                                 : std::function<void(KeyCode, Modifiers)>{ handleKey })
                  .onTap(effectivelyDisabled ? std::function<void()>{} : std::function<void()>{ handleTap })
                  .cornerRadius(cr)
                  .flex(isLink ? 0.f : 1.f, 1.f, 0.f),
              std::move(labelEl)),
  };

  if (isLink) {
    return Element{ std::move(content) };
  }

  return Element{ ScaleAroundCenter{ .scale = *scaleAnim, .child = Element{ std::move(content) } } };
}

} // namespace flux
