#include <Flux/UI/Views/Toggle.hpp>

#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <cmath>

namespace flux {

Toggle::Style resolveStyle(Toggle::Style const &style, Theme const &theme) {
    return Toggle::Style {
        .trackWidth = std::max(1.f, resolveFloat(style.trackWidth, theme.toggleTrackWidth)),
        .trackHeight = std::max(1.f, resolveFloat(style.trackHeight, theme.toggleTrackHeight)),
        .thumbInset = std::max(0.f, resolveFloat(style.thumbInset, theme.toggleThumbInset)),
        .borderWidth = std::max(0.f, resolveFloat(style.borderWidth, theme.toggleBorderWidth)),
        .thumbBorderWidth = std::max(0.f, resolveFloat(style.thumbBorderWidth, theme.toggleThumbBorderWidth)),
        .onColor = resolveColor(style.onColor, theme.toggleOnColor, theme),
        .offColor = resolveColor(style.offColor, theme.toggleOffColor, theme),
        .thumbColor = resolveColor(style.thumbColor, theme.toggleThumbColor, theme),
        .thumbBorderColor = resolveColor(style.thumbBorderColor, theme.toggleThumbBorderColor, theme),
        .borderColor = resolveColor(style.borderColor, theme.toggleBorderColor, theme),
    };
}

Element Toggle::body() const {
    auto theme = useEnvironment<Theme>();

    auto [trackWidth,
          trackHeight,
          thumbInset,
          borderWidth,
          thumbBorderWidth,
          onColor,
          offColor,
          thumbColor,
          thumbBorderColor,
          borderColor] = resolveStyle(style, theme());
    auto disabledColor = theme().disabledTextColor;
    auto focusColor = theme().keyboardFocusIndicatorColor;

    float const maxInset = std::max(0.f, trackHeight * 0.5f - 0.5f);
    thumbInset = std::min(thumbInset, maxInset);
    float const thumbSize = std::max(1.f, trackHeight - 2.f * thumbInset);
    trackWidth = std::max(trackWidth, thumbSize + 2.f * thumbInset);
    float const xOff = thumbInset;
    float const xOn = std::max(xOff, trackWidth - thumbInset - thumbSize);

    Reactive::Signal<bool> focused = useFocus();
    Reactive::Signal<bool> pressed = usePress();
    bool const isDisabled = disabled;
    auto v = value;

    bool const initialOn = v.peek();
    auto thumbXAnim = useAnimation<float>(initialOn ? xOn : xOff);
    auto trackFillAnim = useAnimation<Color>(
        isDisabled ? theme().disabledControlBackgroundColor : initialOn ? onColor : offColor);
    auto scaleAnim = useAnimation<float>(1.f);

    useEffect([theme, style = style, v, pressed, thumbXAnim, trackFillAnim, scaleAnim,
               isDisabled, xOn, xOff]() mutable {
        Theme const &currentTheme = theme();
        Toggle::Style const currentStyle = resolveStyle(style, currentTheme);
        Transition const motion = isDisabled ? Transition::instant()
                                             : Transition::ease(currentTheme.durationMedium);
        Transition const pressMotion = Transition::ease(currentTheme.durationFast);

        bool const on = v();
        float const targetX = on ? xOn : xOff;
        if (std::abs(thumbXAnim.peek() - targetX) > 0.01f) {
            thumbXAnim.set(targetX, motion);
        }

        Color const targetFill = isDisabled ? currentTheme.disabledControlBackgroundColor
                                            : on ? currentStyle.onColor
                                                 : currentStyle.offColor;
        if (trackFillAnim.peek() != targetFill) {
            trackFillAnim.set(targetFill, motion);
        }

        float const targetScale = (pressed() && !isDisabled) ? 0.90f : 1.f;
        if (std::abs(scaleAnim.peek() - targetScale) > 0.001f) {
            scaleAnim.set(targetScale, pressMotion);
        }
    });

    auto handleToggle = [v, onChange = onChange, isDisabled]() {
        if (isDisabled) {
            return;
        }
        bool const next = !v.peek();
        v = next;
        if (onChange) {
            onChange(next);
        }
    };

    auto handleKey = [handleToggle](KeyCode k, Modifiers) {
        if (k == keys::Space || k == keys::Return) {
            handleToggle();
        }
    };

    return ScaleAroundCenter {
        .scale = [scaleAnim] {
            return scaleAnim();
        },
        .child = ZStack {
            .horizontalAlignment = Alignment::Start,
            .verticalAlignment = Alignment::Start,
            .children = flux::children(
                Rectangle {}
                    .fill([trackFillAnim] {
                        return trackFillAnim();
                    })
                    .stroke([focused, focusColor, borderColor, borderWidth] {
                        return StrokeStyle::solid(focused() ? focusColor : borderColor,
                                                  focused() ? std::max(borderWidth, 2.f) : borderWidth);
                    })
                    .size(trackWidth, trackHeight)
                    .cornerRadius(CornerRadius {trackHeight * 0.5f}),
                Rectangle {}
                    .fill(FillStyle::solid(isDisabled ? disabledColor : thumbColor))
                    .stroke(StrokeStyle::solid(thumbBorderColor, thumbBorderWidth))
                    .shadow(isDisabled ? ShadowStyle::none() : ShadowStyle {.radius = theme().shadowRadiusControl, .offset = {0.f, theme().shadowOffsetYControl}, .color = theme().shadowColor})
                    .position([thumbXAnim] {
                        return thumbXAnim();
                    }, thumbInset)
                    .size(thumbSize, thumbSize)
                    .cornerRadius(CornerRadius {thumbSize * 0.5f})
            ),
        }
                     .cursor(isDisabled ? Cursor::Inherit : Cursor::Hand)
                     .focusable(!isDisabled)
                     .onKeyDown(isDisabled ? std::function<void(KeyCode, Modifiers)> {} : std::function<void(KeyCode, Modifiers)> {handleKey})
                     .onTap(isDisabled ? std::function<void()> {} : std::function<void()> {handleToggle}),
    };
}

} // namespace flux
