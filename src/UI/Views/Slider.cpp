#include <Flux/UI/Views/Slider.hpp>

#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace flux {

namespace {

constexpr float kDefaultSliderWidth = 160.f;

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

Slider::Style resolveStyle(Slider::Style const &style, Theme const &theme) {
    return Slider::Style {
        .activeColor = resolveColor(style.activeColor, theme.accentColor, theme),
        .inactiveColor = resolveColor(style.inactiveColor, theme.disabledControlBackgroundColor, theme),
        .thumbColor = resolveColor(style.thumbColor, theme.accentColor, theme),
        .thumbBorderColor = resolveColor(style.thumbBorderColor, theme.separatorColor, theme),
        .trackHeight = resolveFloat(style.trackHeight, theme.sliderTrackHeight),
        .thumbSize = resolveFloat(style.thumbSize, theme.sliderThumbSize),
    };
}

Element Slider::body() const {
    Theme const &theme = useEnvironment<Theme>();

    auto [activeColor,
          inactiveColor,
          thumbColor,
          thumbBorderColor,
          trackHeight,
          thumbSize] = resolveStyle(style, theme);
    auto focusColor = theme.keyboardFocusIndicatorColor;

    auto isDisabled = disabled;
    auto focused = useFocus();
    auto dragging = useState(false);

    Transition const trPress = Transition::ease(theme.durationFast);

    auto thumbScaleAnim = useAnimation<float>(1.f);
    {
        float const target = (*dragging && !isDisabled) ? 1.2f : 1.f;
        if (std::abs(*thumbScaleAnim - target) > 0.001f) {
            thumbScaleAnim.set(target, trPress);
        }
    }

    Rect const bounds = useBounds();
    float const componentWidth = bounds.width > 0.f ? std::max(bounds.width, thumbSize) : std::max(kDefaultSliderWidth, thumbSize);
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

    StrokeStyle thumbStroke = StrokeStyle::solid(thumbBorderColor, 2.f);
    if (focused && !isDisabled) {
        thumbStroke = StrokeStyle::solid(focusColor, 2.f);
    }

    float const componentHeight = std::max(trackHeight, thumbSize * 1.5f);
    float const thumbDiameter = thumbSize * *thumbScaleAnim;
    float const thumbOffset = (thumbSize - thumbDiameter) * 0.5f;

    float const filledWidth = std::max(componentWidth * fraction, trackHeight);
    float const trackY = (componentHeight - trackHeight) * 0.5f;
    float const thumbY = (componentHeight - thumbDiameter) * 0.5f;

    return ZStack {
        .horizontalAlignment = Alignment::Start,
        .verticalAlignment = Alignment::Start,
        .children = flux::children(
            Rectangle {}
                .size(componentWidth, componentHeight),
            Rectangle {}
                .fill(FillStyle::solid(isDisabled ? theme.disabledControlBackgroundColor : inactiveColor))
                .position(0.f, trackY)
                .size(componentWidth, trackHeight)
                .cornerRadius(CornerRadius {trackHeight * 0.5f}),
            Rectangle {}
                .fill(FillStyle::solid(isDisabled ? theme.disabledControlBackgroundColor : activeColor))
                .position(0.f, trackY)
                .size(filledWidth, trackHeight)
                .cornerRadius(CornerRadius {trackHeight * 0.5f}),
            Rectangle {}
                .fill(FillStyle::solid(isDisabled ? theme.disabledTextColor : thumbColor))
                .stroke(isDisabled ? StrokeStyle::solid(theme.disabledTextColor, 1.f) : thumbStroke)
                .shadow(isDisabled ? ShadowStyle::none() : ShadowStyle {.radius = theme.shadowRadiusControl, .offset = {0.f, theme.shadowOffsetYControl}, .color = theme.shadowColor})
                .position(thumbX + thumbOffset, thumbY)
                .size(thumbDiameter, thumbDiameter)
                .cornerRadius(CornerRadius {thumbDiameter * 0.5f})
        ),
    }
        .cursor(isDisabled ? Cursor::Inherit : Cursor::Hand)
        .focusable(!isDisabled)
        .onKeyDown(isDisabled ? std::function<void(KeyCode, Modifiers)> {} : std::function<void(KeyCode, Modifiers)> {handleKey})
        .onPointerDown(isDisabled ? std::function<void(Point)> {} : std::function<void(Point)> {handleDown})
        .onPointerUp(isDisabled ? std::function<void(Point)> {} : std::function<void(Point)> {handleUp})
        .onPointerMove(isDisabled ? std::function<void(Point)> {} : std::function<void(Point)> {handleMove});
}

} // namespace flux
