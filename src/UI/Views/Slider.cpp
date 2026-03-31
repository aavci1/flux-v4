#include <Flux/UI/Views/Slider.hpp>

#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <cmath>

namespace flux {

namespace {

float snapToStep(float raw, float step, float lo, float hi) {
  if (step <= 0.f) {
    return std::clamp(raw, lo, hi);
  }
  float const snapped = std::round((raw - lo) / step) * step + lo;
  return std::clamp(snapped, lo, hi);
}

float fractionForValue(float val, float lo, float hi) {
  if (hi <= lo) {
    return 0.f;
  }
  return std::clamp((val - lo) / (hi - lo), 0.f, 1.f);
}

} // namespace

Element Slider::body() const {
  FluxTheme const& theme = useEnvironment<FluxTheme>();

  Color const activeC = resolveColor(activeColor, theme.colorAccent);
  Color const inactiveC = resolveColor(inactiveColor, theme.colorSurfaceDisabled);
  Color const thumbC = resolveColor(thumbColor, Colors::white);
  Color const thumbBrdC = resolveColor(thumbBorder, theme.colorBorder);
  Color const focusC = resolveColor(focusColor, theme.colorBorderFocus);

  float const trkH = std::max(trackHeight, 2.f);
  float const thSz = std::max(thumbSize, 8.f);
  float const compH = height > 0.f ? height : (thSz + 8.f);

  bool const focused = useFocus();
  bool const isDisabled = disabled;
  auto dragging = useState(false);

  Transition const tr =
      theme.reducedMotion ? Transition::instant() : Transition::ease(theme.durationFast);

  auto thumbScaleAnim = useAnimated<float>(1.f);
  {
    float const target = (*dragging && !isDisabled) ? 1.15f : 1.f;
    if (std::abs(*thumbScaleAnim - target) > 0.001f) {
      thumbScaleAnim.set(target, tr);
    }
  }

  std::optional<Rect> const layoutRect = useLayoutRect();
  float const componentWidth = layoutRect ? layoutRect->width : 200.f;
  float const usableWidth = std::max(componentWidth - thSz, 1.f);

  float const fraction = fractionForValue(*value, min, max);
  float const thumbX = fraction * usableWidth;
  float const filledWidth = thumbX + thSz * 0.5f;

  State<float> val = value;
  float const lo = min;
  float const hi = max;
  float const stp = step;
  float const thSzC = thSz;
  std::function<void(float)> onCh = onChange;

  auto applyPointer = [val, lo, hi, stp, thSzC, usableWidth, onCh](float localX) {
    float const frac = std::clamp((localX - thSzC * 0.5f) / usableWidth, 0.f, 1.f);
    float const raw = lo + frac * (hi - lo);
    float const snapped = snapToStep(raw, stp, lo, hi);
    if (snapped != *val) {
      val = snapped;
      if (onCh) {
        onCh(snapped);
      }
    }
  };

  auto handleDown = [applyPointer, dragging, isDisabled](Point local) {
    if (isDisabled) {
      return;
    }
    dragging = true;
    applyPointer(local.x);
  };

  auto handleMove = [applyPointer, dragging, isDisabled](Point local) {
    if (isDisabled || !*dragging) {
      return;
    }
    applyPointer(local.x);
  };

  auto handleUp = [dragging](Point) { dragging = false; };

  auto handleKey = [val, lo, hi, stp, onCh, isDisabled](KeyCode k, Modifiers mods) {
    if (isDisabled) {
      return;
    }
    float const range = hi - lo;
    float const baseStep = stp > 0.f ? stp : range * 0.01f;
    float const keyStep = any(mods & Modifiers::Shift) ? baseStep * 10.f : baseStep;

    float next = *val;
    if (k == keys::RightArrow || k == keys::UpArrow) {
      next = std::min(*val + keyStep, hi);
    } else if (k == keys::LeftArrow || k == keys::DownArrow) {
      next = std::max(*val - keyStep, lo);
    } else {
      return;
    }
    if (stp > 0.f) {
      next = snapToStep(next, stp, lo, hi);
    }
    if (next != *val) {
      val = next;
      if (onCh) {
        onCh(next);
      }
    }
  };

  StrokeStyle thumbStroke = StrokeStyle::solid(thumbBrdC, 1.f);
  if (focused && !isDisabled) {
    thumbStroke = StrokeStyle::solid(focusC, 2.f);
  }

  CornerRadius const trackCr{trkH * 0.5f};
  float const thumbDia = thSz * *thumbScaleAnim;
  CornerRadius const thumbCr{thumbDia * 0.5f};
  float const thumbOffset = (thSz - thumbDia) * 0.5f;

  float const trackY = (compH - trkH) * 0.5f;
  float const thumbY = (compH - thumbDia) * 0.5f;

  return Element{ZStack{
      .hAlign = HorizontalAlignment::Leading,
      .vAlign = VerticalAlignment::Top,
      .children = {
          // Full-height invisible hit target + intrinsic height (pointer + keyboard).
          Rectangle{
              .frame = {0.f, 0.f, componentWidth, compH},
              .cornerRadius = {},
              .fill = FillStyle::none(),
              .stroke = StrokeStyle::none(),
              .flexGrow = flexGrow,
              .flexShrink = flexShrink,
              .minSize = minSize,
              .onPointerDown =
                  isDisabled ? nullptr : std::function<void(Point)>{handleDown},
              .onPointerUp = isDisabled ? nullptr : std::function<void(Point)>{handleUp},
              .onPointerMove =
                  isDisabled ? nullptr : std::function<void(Point)>{handleMove},
              .focusable = !isDisabled,
              .onKeyDown    = isDisabled ? nullptr : std::function<void(KeyCode, Modifiers)>{handleKey},
              .cursor       = isDisabled ? Cursor::Inherit : Cursor::Hand,
          },
          // Inactive track (visual only; events hit the layer below).
          Rectangle{
              .frame = {0.f, trackY, componentWidth, trkH},
              .cornerRadius = trackCr,
              .fill = FillStyle::solid(isDisabled ? theme.colorSurfaceDisabled : inactiveC),
              .stroke = StrokeStyle::none(),
          },
          // Active track (filled portion).
          Rectangle{
              .frame = {0.f, trackY, std::max(filledWidth, trkH), trkH},
              .cornerRadius = trackCr,
              .fill = FillStyle::solid(isDisabled ? theme.colorTextDisabled : activeC),
              .stroke = StrokeStyle::none(),
          },
          // Thumb.
          Rectangle{
              .frame = {thumbX + thumbOffset, thumbY, thumbDia, thumbDia},
              .cornerRadius = thumbCr,
              .fill = FillStyle::solid(isDisabled ? theme.colorSurfaceDisabled : thumbC),
              .stroke = isDisabled ? StrokeStyle::solid(theme.colorBorder, 1.f) : thumbStroke,
              .cursor = isDisabled ? Cursor::Inherit : Cursor::Hand,
              .cursorPassthrough = !isDisabled,
          },
      },
  }};
}

} // namespace flux
