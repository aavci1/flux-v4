#include <Flux/UI/Views/Slider.hpp>

#include <Flux/Core/KeyCodes.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

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

Slider::Style resolveStyle(Slider::Style const& style, FluxTheme const& theme) {
    return Slider::Style {
        .activeColor = resolveColor(style.activeColor, theme.colorAccent),
        .inactiveColor = resolveColor(style.inactiveColor, theme.colorSurfaceDisabled),
        .thumbColor = resolveColor(style.thumbColor, theme.sliderThumbColor),
        .thumbBorderColor = resolveColor(style.thumbBorderColor, theme.sliderThumbBorderColor),
        .trackHeight = resolveFloat(style.trackHeight, theme.sliderTrackHeight),
        .thumbSize = resolveFloat(style.thumbSize, theme.sliderThumbSize),
    };
}

Element Slider::body() const {
    FluxTheme const& theme = useEnvironment<FluxTheme>();

    auto [
        activeColor,
        inactiveColor,
        thumbColor,
        thumbBorderColor,
        trackHeight,
        thumbSize
    ] = resolveStyle(style, theme);
    auto focusColor = theme.colorBorderFocus;

    auto isDisabled = disabled;
    auto focused = useFocus();
    auto dragging = useState(false);

    Transition const trInstant = Transition::instant();
    Transition const trPress = theme.reducedMotion ? trInstant : Transition::ease(theme.durationFast);

    auto thumbScaleAnim = useAnimated<float>(1.f);
    {
        float const target = (*dragging && !isDisabled) ? 1.2f : 1.f;
        if (std::abs(*thumbScaleAnim - target) > 0.001f) {
            thumbScaleAnim.set(target, trPress);
        }
    }

    LayoutConstraints const* const lc = useLayoutConstraints();
    float capW = std::numeric_limits<float>::infinity();
    if (lc && std::isfinite(lc->maxWidth) && lc->maxWidth > 0.f) {
        capW = lc->maxWidth;
    }
    std::optional<Rect> const layoutRect = useLayoutRect();
    Rect const slot = Runtime::current() ? Runtime::current()->buildSlotRect() : Rect{};
    float componentWidth = thumbSize;
    if (slot.width > 0.f) {
        componentWidth = std::min(slot.width, capW);
    } else if (layoutRect) {
        componentWidth = std::min(layoutRect->width, capW);
    } else if (std::isfinite(capW) && capW > 0.f) {
        componentWidth = capW;
    } else {
        componentWidth = std::min(componentWidth, capW);
    }
    float const usableWidth = std::max(componentWidth - thumbSize, 1.f);

    float const fraction = fractionForValue(*value, min, max);
    float const thumbX = fraction * usableWidth;

    State<float> val = value;
    float const lo = min;
    float const hi = max;
    float const stp = step;
    float const thumbSizeC = thumbSize;
    std::function<void(float)> onCh = onChange;

    auto applyPointer = [val, lo, hi, stp, thumbSizeC, usableWidth, onCh](float localX) {
        float const frac = std::clamp((localX - thumbSizeC * 0.5f) / usableWidth, 0.f, 1.f);
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

    StrokeStyle thumbStroke = StrokeStyle::solid(thumbBorderColor, 1.f);
    if (focused && !isDisabled) {
        thumbStroke = StrokeStyle::solid(focusColor, 2.f);
    }

    float const componentHeight = std::max(trackHeight, thumbSize * 1.5f);
    float const thumbDiameter = thumbSize * *thumbScaleAnim;
    float const thumbOffset = (thumbSize - thumbDiameter) * 0.5f;

    float const trackWidth = componentWidth - thumbSize;
    float const filledWidth = std::max(trackWidth * fraction, trackHeight);
    float const trackY = (componentHeight - trackHeight) * 0.5f;
    float const thumbY = (componentHeight - thumbDiameter) * 0.5f;

    return ZStack {
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children = {
            Rectangle {
                .offsetX = 0.f, .offsetY = 0.f, .width = componentWidth, .height = componentHeight,
            }
                .cursor(isDisabled ? Cursor::Inherit : Cursor::Hand)
                .focusable(!isDisabled)
                .onKeyDown(isDisabled ? std::function<void(KeyCode, Modifiers)>{}
                                      : std::function<void(KeyCode, Modifiers)>{handleKey})
                .onPointerDown(isDisabled ? std::function<void(Point)>{}
                                          : std::function<void(Point)> {handleDown})
                .onPointerUp(isDisabled ? std::function<void(Point)>{}
                                        : std::function<void(Point)> {handleUp})
                .onPointerMove(isDisabled ? std::function<void(Point)>{}
                                          : std::function<void(Point)> {handleMove}),
            Rectangle {
                .offsetX = thumbSize * 0.5f, .offsetY = trackY, .width = trackWidth, .height = trackHeight,
                .fill = FillStyle::solid(isDisabled ? theme.colorSurfaceDisabled : inactiveColor),
                .stroke = StrokeStyle::none(),
            }
                .cornerRadius(CornerRadius{trackHeight * 0.5f}),
            Rectangle {
                .offsetX = thumbSize * 0.5f, .offsetY = trackY, .width = filledWidth, .height = trackHeight,
                .fill = FillStyle::solid(isDisabled ? theme.colorSurfaceDisabled : activeColor),
                .stroke = StrokeStyle::none(),
            }
                .cornerRadius(CornerRadius{trackHeight * 0.5f}),
            Rectangle {
                .offsetX = thumbX + thumbOffset, .offsetY = thumbY, .width = thumbDiameter, .height = thumbDiameter,
                .fill = FillStyle::solid(isDisabled ? theme.colorTextDisabled : thumbColor),
                .stroke = isDisabled ? StrokeStyle::solid(theme.colorTextDisabled, 1.f) : thumbStroke,
            }
                .cursor(isDisabled ? Cursor::Inherit : Cursor::Hand)
                .cursorPassthrough(!isDisabled)
                .cornerRadius(CornerRadius{thumbDiameter * 0.5f}),
        },
    };
}

} // namespace flux
