#include <Flux/UI/Views/Toggle.hpp>

#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <cmath>

namespace flux {

Toggle::Style resolveStyle(Toggle::Style const& style, Theme const& theme) {
  return Toggle::Style {
    .trackWidth = resolveFloat(style.trackWidth, theme.toggleTrackWidth),
    .trackHeight = resolveFloat(style.trackHeight, theme.toggleTrackHeight),
    .thumbInset = resolveFloat(style.thumbInset, theme.toggleThumbInset),
    .borderWidth = resolveFloat(style.borderWidth, theme.toggleBorderWidth),
    .thumbBorderWidth = resolveFloat(style.thumbBorderWidth, theme.toggleThumbBorderWidth),
    .onColor = resolveColor(style.onColor, theme.toggleOnColor),
    .offColor = resolveColor(style.offColor, theme.toggleOffColor),
    .thumbColor = resolveColor(style.thumbColor, theme.toggleThumbColor),
    .thumbBorderColor = resolveColor(style.thumbBorderColor, theme.toggleThumbBorderColor),
    .borderColor = resolveColor(style.borderColor, theme.toggleBorderColor),
  };
}

Element Toggle::body() const {
  Theme const &theme = useEnvironment<Theme>();

  auto [
    trackWidth,
    trackHeight,
    thumbInset,
    borderWidth,
    thumbBorderWidth,
    onColor,
    offColor,
    thumbColor,
    thumbBorderColor,
    borderColor
  ] = resolveStyle(style, theme);
  auto disabledColor = theme.colorTextDisabled;
  auto focusColor = theme.colorBorderFocus;

  float const thumbSize = trackHeight - 2.f * thumbInset;
  float const xOff = thumbInset;
  float const xOn = trackWidth - thumbInset - thumbSize;

  bool const isOn = *value;
  bool const focused = useFocus();
  bool const pressed = usePress();
  bool const isDisabled = disabled;


  // ── Animations ────────────────────────────────────────────────────────────

  Transition const trInstant = Transition::instant();
  Transition const trMotion = theme.reducedMotion ? trInstant : Transition::ease(theme.durationMedium);
  Transition const tr = isDisabled ? trInstant : trMotion;
  Transition const trPress = theme.reducedMotion ? trInstant : Transition::ease(theme.durationFast);

  auto thumbXAnim = useAnimated<float>(isOn ? xOn : xOff);
  {
    float const targetX = isOn ? xOn : xOff;
    if (std::abs(*thumbXAnim - targetX) > 0.01f) {
      thumbXAnim.set(targetX, tr);
    }
  }

  auto trackFillAnim = useAnimated<Color>(isOn ? onColor : offColor);
  {
    Color const targetFill = isDisabled ? theme.colorSurfaceDisabled : isOn ? onColor : offColor;
    if (*trackFillAnim != targetFill) {
      trackFillAnim.set(targetFill, tr);
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
  auto oc = onChange;

  auto handleToggle = [v, oc, isDisabled]() {
    if (isDisabled) {
      return;
    }
    bool const next = !*v;
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

  return ScaleAroundCenter{
      .scale = *scaleAnim,
      .child = ZStack {
          .horizontalAlignment = Alignment::Start,
          .verticalAlignment = Alignment::Start,
          .children = flux::children(
            Rectangle{}
              .fill(FillStyle::solid(*trackFillAnim))
              .stroke(StrokeStyle::solid(focused ? focusColor : borderColor, borderWidth))
              .size(trackWidth, trackHeight)
              .cornerRadius(CornerRadius{trackHeight * 0.5f}),
            Rectangle{}
              .fill(FillStyle::solid(isDisabled ? disabledColor : thumbColor))
              .stroke(StrokeStyle::solid(thumbBorderColor, thumbBorderWidth))
              .shadow(isDisabled ? ShadowStyle::none()
                                 : ShadowStyle{.radius = theme.shadowRadiusControl,
                                               .offset = {0.f, theme.shadowOffsetYControl},
                                               .color = theme.shadowColor})
              .position(*thumbXAnim, thumbInset)
              .size(thumbSize, thumbSize)
              .cornerRadius(CornerRadius{thumbSize * 0.5f})
          ),
      }.cursor(isDisabled ? Cursor::Inherit : Cursor::Hand)
      .focusable(!isDisabled)
      .onKeyDown(isDisabled ? std::function<void(KeyCode, Modifiers)>{}
                            : std::function<void(KeyCode, Modifiers)>{handleKey})
      .onTap(isDisabled ? std::function<void()>{} : std::function<void()>{handleToggle}),
  };
}

} // namespace flux
