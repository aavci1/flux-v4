#include <Flux/Core/Window.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/Transition.hpp>
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

ButtonColors deriveColors(ButtonVariant variant, Color accent, Color destructive) {
  switch (variant) {
  case ButtonVariant::Primary:
    return {
        .fill = accent,
        .fillHover = lighten(accent, 0.08f),
        .fillPress = darken(accent, 0.08f),
        .label = Colors::white,
        .labelHover = Colors::white,
        .labelPress = Colors::white,
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
        .label = Colors::white,
        .labelHover = Colors::white,
        .labelPress = Colors::white,
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

  ButtonColors const colors = deriveColors(variant, accentColor, destructiveColor);

  auto fillAnim = useAnimated<Color>(colors.fill);
  if (!isLink) {
    Color const fillTarget = effectivelyDisabled ? Color::hex(0xE0E0E4)
                                                 : pressed ? colors.fillPress
                                                 : hovered ? colors.fillHover
                                                           : colors.fill;
    if (*fillAnim != fillTarget) {
      fillAnim.set(fillTarget, Transition::ease(0.12f));
    }
  }

  auto labelAnim = useAnimated<Color>(colors.label);
  {
    Color const labelTarget =
        effectivelyDisabled
            ? Color::hex(0xAAAAAA)
            : (isLink ? (pressed ? colors.labelPress : hovered ? colors.labelHover : colors.label) : colors.label);
    if (*labelAnim != labelTarget) {
      labelAnim.set(labelTarget, Transition::ease(0.12f));
    }
  }

  auto scaleAnim = useAnimated<float>(1.f);
  if (!isLink) {
    float const scaleTarget = (pressed && !effectivelyDisabled) ? 0.97f : 1.f;
    if (std::abs(*scaleAnim - scaleTarget) > 0.001f) {
      scaleAnim.set(scaleTarget, Transition::ease(0.10f));
    }
  }

  if (!actionName.empty() && onTap) {
    useWindowAction(actionName, [onTap = onTap, effectivelyDisabled] {
      if (!effectivelyDisabled) {
        onTap();
      }
    }, [effectivelyDisabled] { return !effectivelyDisabled; });
  }

  float const h = isLink ? 0.f : (height > 0.f ? height : 36.f);
  float const effPaddingH = isLink ? 0.f : paddingH;
  float const effFlexGrow = isLink ? 0.f : flexGrow;

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
  CornerRadius const cr = isLink ? CornerRadius{ 2.f } : cornerRadius;

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

  auto content = ZStack{
    .hAlign = HorizontalAlignment::Center,
    .vAlign = VerticalAlignment::Center,
    .children =
        {
            Rectangle{
                .frame = { 0.f, 0.f, 0.f, h },
                .cornerRadius = cr,
                .fill = FillStyle::solid(*fillAnim),
                .stroke = stroke,
                .flexGrow = effFlexGrow,
                .flexShrink = flexShrink,
                .minSize = minSize,
                .onTap = effectivelyDisabled ? nullptr : std::function<void()>{ handleTap },
                .focusable = !effectivelyDisabled,
                .onKeyDown = effectivelyDisabled ? nullptr : std::function<void(KeyCode, Modifiers)>{ handleKey },
                .cursor = effectivelyDisabled ? Cursor::Inherit : Cursor::Hand,
            },
            Text{
                .text = label,
                .font = font,
                .color = *labelAnim,
                .horizontalAlignment = isLink ? HorizontalAlignment::Leading : HorizontalAlignment::Center,
                .verticalAlignment = VerticalAlignment::Center,
                .padding = effPaddingH,
            },
        },
  };

  if (isLink) {
    return Element{ std::move(content) };
  }

  return Element{ ScaleAroundCenter{ .scale = *scaleAnim, .child = Element{ std::move(content) } } };
}

} // namespace flux
