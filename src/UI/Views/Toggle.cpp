#include <Flux/UI/Views/Toggle.hpp>

#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <cmath>

namespace flux {

Element Toggle::body() const {
  FluxTheme const &theme = useEnvironment<FluxTheme>();

  float const tw = trackWidth > 0.f ? trackWidth : 44.f;
  float const th = trackHeight > 0.f ? trackHeight : 26.f;
  float const inset = std::max(thumbInset, 1.f);
  float const thumbDia = th - 2.f * inset;
  float const xOff = inset;
  float const xOn = tw - inset - thumbDia;

  Color const onC = resolveColor(onColor, theme.colorAccent);
  Color const offC = resolveColor(offColor, theme.colorSurfaceDisabled);
  Color const thumbC = resolveColor(thumbColor, Colors::white);
  Color const borderC = resolveColor(borderColor, theme.colorBorder);
  Color const focusC = resolveColor(focusColor, theme.colorBorderFocus);

  bool const isOn = *value;
  bool const focused = useFocus();
  bool const isDisabled = disabled;

  Transition const trInstant = Transition::instant();
  Transition const trMotion =
      theme.reducedMotion ? trInstant : Transition::ease(theme.durationMedium);
  Transition const tr = isDisabled ? trInstant : trMotion;

  auto thumbXAnim = useAnimated<float>(isOn ? xOn : xOff);
  auto trackFillAnim = useAnimated<Color>(isOn ? onC : offC);
  auto trackBorderAnim =
      useAnimated<Color>(isOn ? Colors::transparent : borderC);

  {
    float const targetX = isOn ? xOn : xOff;
    if (std::abs(*thumbXAnim - targetX) > 0.01f) {
      thumbXAnim.set(targetX, tr);
    }
  }
  {
    Color const targetFill = isDisabled ? theme.colorSurfaceDisabled
                             : isOn     ? onC
                                        : offC;
    if (*trackFillAnim != targetFill) {
      trackFillAnim.set(targetFill, tr);
    }
  }
  {
    Color const targetBorder = isOn ? Colors::transparent : borderC;
    if (*trackBorderAnim != targetBorder) {
      trackBorderAnim.set(targetBorder, tr);
    }
  }

  State<bool> val = value;
  std::function<void(bool)> onCh = onChange;

  auto handleToggle = [val, onCh, isDisabled]() {
    if (isDisabled) {
      return;
    }
    bool const next = !*val;
    val = next;
    if (onCh) {
      onCh(next);
    }
  };

  auto handlePointerDown = [handleToggle](Point) { handleToggle(); };

  auto handleKey = [handleToggle](KeyCode k, Modifiers) {
    if (k == keys::Space || k == keys::Return) {
      handleToggle();
    }
  };

  StrokeStyle trackStroke{};
  if (isDisabled) {
    trackStroke = StrokeStyle::none();
  } else if (focused) {
    trackStroke = StrokeStyle::solid(focusC, 2.f);
  } else {
    trackStroke = StrokeStyle::solid(*trackBorderAnim, 1.f);
  }

  CornerRadius const trackCr{th * 0.5f};
  CornerRadius const thumbCr{thumbDia * 0.5f};

  return Element{ZStack{
      .hAlign = HorizontalAlignment::Leading,
      .vAlign = VerticalAlignment::Center,
      .children =
          {
              Rectangle{
                  .frame = {0.f, 0.f, tw, th},
                  .cornerRadius = trackCr,
                  .fill = FillStyle::solid(*trackFillAnim),
                  .stroke = trackStroke,
                  .flexGrow = flexGrow,
                  .flexShrink = flexShrink,
                  .minSize = minSize,
                  .onPointerDown =
                      isDisabled
                          ? nullptr
                          : std::function<void(Point)>{handlePointerDown},
                  .focusable = !isDisabled,
                  .onKeyDown =
                      isDisabled
                          ? nullptr
                          : std::function<void(KeyCode, Modifiers)>{handleKey},
                  .cursor = isDisabled ? Cursor::Inherit : Cursor::Hand,
              },
              Rectangle{
                  .frame = {*thumbXAnim, inset, thumbDia, thumbDia},
                  .cornerRadius = thumbCr,
                  .fill = FillStyle::solid(isDisabled ? theme.colorTextDisabled
                                                      : thumbC),
                  .stroke = StrokeStyle::none(),
                  .cursor = isDisabled ? Cursor::Inherit : Cursor::Hand,
                  .cursorPassthrough = true,
              },
          },
  }};
}

} // namespace flux
