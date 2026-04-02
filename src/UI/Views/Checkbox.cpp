#include <Flux/UI/Views/Checkbox.hpp>

#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Icon.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <cmath>

namespace flux {

Checkbox::Style resolveStyle(Checkbox::Style const& style, FluxTheme const& theme) {
  return Checkbox::Style {
    .boxSize = resolveFloat(style.boxSize, theme.checkboxBoxSize),
    .cornerRadius = resolveFloat(style.cornerRadius, theme.checkboxCornerRadius),
    .borderWidth = resolveFloat(style.borderWidth, theme.checkboxBorderWidth),
    .checkedColor = resolveColor(style.checkedColor, theme.checkboxCheckedColor),
    .uncheckedColor = resolveColor(style.uncheckedColor, theme.checkboxUncheckedColor),
    .checkColor = resolveColor(style.checkColor, theme.checkboxCheckColor),
    .borderColor = resolveColor(style.borderColor, theme.checkboxBorderColor),
  };
}

Element Checkbox::body() const {
  FluxTheme const& theme = useEnvironment<FluxTheme>();

  auto [
    boxSize,
    cornerRadius,
    borderWidth,
    checkedColor,
    uncheckedColor,
    checkColor,
    borderColor
  ] = resolveStyle(style, theme);
  auto disabledColor = theme.colorTextDisabled;
  auto focusColor = theme.colorBorderFocus;

  float const iconSz = boxSize * 0.6f;

  bool const isOn = *value;
  bool const showFilled = isOn || indeterminate;
  bool const focused = useFocus();
  bool const pressed = usePress();
  bool const isDisabled = disabled;

  Transition const trInstant = Transition::instant();
  Transition const trMotion = theme.reducedMotion ? trInstant : Transition::ease(theme.durationMedium);
  Transition const tr = isDisabled ? trInstant : trMotion;
  Transition const trPress = theme.reducedMotion ? trInstant : Transition::ease(theme.durationFast);

  auto boxFillAnim = useAnimated<Color>(showFilled ? checkedColor : uncheckedColor);
  {
    Color const target = isDisabled ? theme.colorSurfaceDisabled : showFilled ? checkedColor : uncheckedColor;
    if (*boxFillAnim != target) {
      boxFillAnim.set(target, tr);
    }
  }

  Color const iconTransparent = Color {checkColor.r, checkColor.g, checkColor.b, 0.f};
  auto iconColorAnim = useAnimated<Color>(showFilled ? checkColor : iconTransparent);
  {
    Color const target = !showFilled ? iconTransparent : isDisabled ? disabledColor : checkColor;
    if (*iconColorAnim != target) {
      iconColorAnim.set(target, tr);
    }
  }

  auto scaleAnim = useAnimated<float>(1.f);
  {
    float const target = (pressed && !isDisabled) ? 0.90f : 1.f;
    if (std::abs(*scaleAnim - target) > 0.001f) {
      scaleAnim.set(target, trPress);
    }
  }

  auto v = value;
  auto i = indeterminate;
  auto oc = onChange;
  auto handleToggle = [v, i, oc, isDisabled]() {
    if (isDisabled) {
      return;
    }
    bool const next = i ? true : !*v;
    v = next;
    if (oc) {
      oc(next);
    }
  };

  auto handleKey = [handleToggle](KeyCode k, Modifiers) {
    if (k == keys::Space || k == keys::Return) {
      handleToggle();
    }
  };

  StrokeStyle boxStroke = StrokeStyle::solid(borderColor, borderWidth);
  if (focused && !isDisabled) {
    boxStroke = StrokeStyle::solid(focusColor, borderWidth);
  }

  IconName const iconName = indeterminate ? IconName::HorizontalRule : IconName::Check;

  return ScaleAroundCenter {
    .scale = *scaleAnim,
    .child = ZStack {
      .hAlign = HorizontalAlignment::Center,
      .vAlign = VerticalAlignment::Center,
      .children = {
          Rectangle {
              .offsetX = 0.f, .offsetY = 0.f, .width = boxSize, .height = boxSize,
              .fill = FillStyle::solid(*boxFillAnim),
              .stroke = boxStroke,
          }
              .cursor(isDisabled ? Cursor::Inherit : Cursor::Hand)
              .focusable(!isDisabled)
              .onKeyDown(isDisabled ? std::function<void(KeyCode, Modifiers)>{}
                                    : std::function<void(KeyCode, Modifiers)>{handleKey})
              .onTap(isDisabled ? std::function<void()>{} : std::function<void()> {handleToggle})
              .cornerRadius(CornerRadius{cornerRadius})
              .flex(flexGrow, flexShrink, minSize),
          Icon {
              .name = iconName,
              .size = iconSz,
              .color = *iconColorAnim,
          },
      },
    },
  };
}

} // namespace flux
