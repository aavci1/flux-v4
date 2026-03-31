#include <Flux/UI/Views/Checkbox.hpp>

#include <Flux/Core/Cursor.hpp>
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

Element Checkbox::body() const {
  FluxTheme const& theme = useEnvironment<FluxTheme>();

  float const sz = boxSize > 0.f ? boxSize : 20.f;
  float const cr = resolveFloat(cornerRadius, theme.radiusXSmall);
  float const iconSz = sz * 0.6f;

  Color const checkedC = resolveColor(checkedColor, theme.colorAccent);
  Color const uncheckedC = resolveColor(uncheckedColor, Colors::transparent);
  Color const borderC = resolveColor(borderColor, theme.colorBorder);
  Color const iconC = resolveColor(iconColor, theme.colorOnAccent);
  Color const focusC = resolveColor(focusColor, theme.colorBorderFocus);

  bool const isOn = *value;
  bool const showFilled = isOn || indeterminate;
  bool const focused = useFocus();
  bool const pressed = usePress();
  bool const isDisabled = disabled;

  Transition const tr =
      theme.reducedMotion ? Transition::instant() : Transition::ease(theme.durationFast);

  auto boxFillAnim = useAnimated<Color>(showFilled ? checkedC : uncheckedC);
  {
    Color const target = isDisabled ? theme.colorSurfaceDisabled
                     : showFilled ? checkedC
                                  : uncheckedC;
    if (*boxFillAnim != target) {
      boxFillAnim.set(target, tr);
    }
  }

  auto boxBorderAnim = useAnimated<Color>(showFilled ? Colors::transparent : borderC);
  {
    Color const target = showFilled ? Colors::transparent : borderC;
    if (*boxBorderAnim != target) {
      boxBorderAnim.set(target, tr);
    }
  }

  Color const iconTransparent = Color{iconC.r, iconC.g, iconC.b, 0.f};
  auto iconColorAnim = useAnimated<Color>(showFilled ? iconC : iconTransparent);
  {
    Color const target =
        !showFilled ? iconTransparent : isDisabled ? theme.colorTextDisabled : iconC;
    if (*iconColorAnim != target) {
      iconColorAnim.set(target, tr);
    }
  }

  auto scaleAnim = useAnimated<float>(1.f);
  {
    float const target = (pressed && !isDisabled) ? 0.90f : 1.f;
    if (std::abs(*scaleAnim - target) > 0.001f) {
      scaleAnim.set(target, tr);
    }
  }

  State<bool> val = value;
  bool const indet = indeterminate;
  std::function<void(bool)> onCh = onChange;

  auto handleToggle = [val, indet, onCh, isDisabled]() {
    if (isDisabled) {
      return;
    }
    bool const next = indet ? true : !*val;
    val = next;
    if (onCh) {
      onCh(next);
    }
  };

  auto handleKey = [handleToggle](KeyCode k, Modifiers) {
    if (k == keys::Space || k == keys::Return) {
      handleToggle();
    }
  };

  StrokeStyle boxStroke = StrokeStyle::solid(*boxBorderAnim, 1.5f);
  if (focused && !isDisabled) {
    boxStroke = StrokeStyle::solid(focusC, 2.f);
  }

  IconName const iconName =
      indeterminate ? IconName::HorizontalRule : IconName::Check;

  CornerRadius const boxCr{cr};

  auto content = ZStack{
      .hAlign = HorizontalAlignment::Center,
      .vAlign = VerticalAlignment::Center,
      .children =
          {
              Rectangle{
                  .frame = {0.f, 0.f, sz, sz},
                  .cornerRadius = boxCr,
                  .fill = FillStyle::solid(*boxFillAnim),
                  .stroke = boxStroke,
                  .flexGrow = flexGrow,
                  .flexShrink = flexShrink,
                  .minSize = minSize,
                  .onTap = isDisabled ? nullptr : std::function<void()>{handleToggle},
                  .focusable = !isDisabled,
                  .onKeyDown =
                      isDisabled ? nullptr : std::function<void(KeyCode, Modifiers)>{handleKey},
                  .cursor = isDisabled ? Cursor::Inherit : Cursor::Hand,
              },
              Icon{
                  .name = iconName,
                  .size = iconSz,
                  .color = *iconColorAnim,
              },
          },
  };

  return Element{ScaleAroundCenter{
      .scale = *scaleAnim,
      .child = Element{std::move(content)},
  }};
}

} // namespace flux
