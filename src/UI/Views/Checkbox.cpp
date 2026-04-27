#include <Flux/UI/Views/Checkbox.hpp>

#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Icon.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>

namespace flux {

Checkbox::Style resolveStyle(Checkbox::Style const &style, Theme const &theme) {
    return Checkbox::Style {
        .boxSize = resolveFloat(style.boxSize, theme.checkboxBoxSize),
        .cornerRadius = resolveFloat(style.cornerRadius, theme.checkboxCornerRadius),
        .borderWidth = resolveFloat(style.borderWidth, theme.checkboxBorderWidth),
        .checkedColor = resolveColor(style.checkedColor, theme.checkboxCheckedColor, theme),
        .uncheckedColor = resolveColor(style.uncheckedColor, theme.checkboxUncheckedColor, theme),
        .checkColor = resolveColor(style.checkColor, theme.checkboxCheckColor, theme),
        .borderColor = resolveColor(style.borderColor, theme.checkboxBorderColor, theme),
    };
}

Element Checkbox::body() const {
    auto theme = useEnvironmentReactive<ThemeKey>();

    auto [boxSize,
          cornerRadius,
          borderWidth,
          checkedColor,
          uncheckedColor,
          checkColor,
          borderColor] = resolveStyle(style, theme());
    auto disabledColor = theme().disabledTextColor;
    auto focusColor = theme().keyboardFocusIndicatorColor;

    float const iconSz = boxSize * 0.6f;

    Reactive::Signal<bool> focused = useFocus();
    Reactive::Signal<bool> pressed = usePress();
    Reactive::Bindable<bool> const indeterminateBinding = indeterminate;
    Reactive::Bindable<bool> const disabledBinding = disabled;
    bool const isDisabled = disabledBinding.evaluate();

    Color const iconTransparent = Color {checkColor.r, checkColor.g, checkColor.b, 0.f};

    auto v = value;
    auto handleToggle = [v, indeterminateBinding, disabledBinding, onChange = onChange]() {
        if (disabledBinding.evaluate()) {
            return;
        }
        bool const next = indeterminateBinding.evaluate() ? true : !v.get();
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

    Reactive::Bindable<StrokeStyle> const boxStroke{[focused, disabledBinding, focusColor,
                                                     borderColor, borderWidth] {
        return focused.get() && !disabledBinding.evaluate()
                   ? StrokeStyle::solid(focusColor, std::max(borderWidth, 2.f))
                   : StrokeStyle::solid(borderColor, borderWidth);
    }};

    auto motion = [theme] {
        return Transition::ease(theme().durationFast);
    };
    auto boxFillTarget = [v, indeterminateBinding, disabledBinding, checkedColor,
                          uncheckedColor, theme] {
        bool const showFilled = v() || indeterminateBinding.evaluate();
        return disabledBinding.evaluate()
                   ? theme().disabledControlBackgroundColor
                   : showFilled ? checkedColor : uncheckedColor;
    };

    auto iconColorTarget = [v, indeterminateBinding, disabledBinding, disabledColor,
                            checkColor, iconTransparent] {
        bool const showFilled = v() || indeterminateBinding.evaluate();
        if (!showFilled) {
            return iconTransparent;
        }
        return disabledBinding.evaluate() ? disabledColor : checkColor;
    };
    auto scaleTarget = [pressed, disabledBinding] {
        return pressed() && !disabledBinding.evaluate() ? 0.92f : 1.f;
    };
    auto boxFillAnim = useAnimatedValue<Color>(boxFillTarget(), boxFillTarget, motion);
    auto iconColorAnim = useAnimatedValue<Color>(iconColorTarget(), iconColorTarget, motion);
    auto scaleAnim = useAnimatedValue<float>(1.f, scaleTarget, motion);

    Reactive::Bindable<IconName> const iconName{[indeterminateBinding] {
        return indeterminateBinding.evaluate() ? IconName::HorizontalRule : IconName::Check;
    }};

    return ScaleAroundCenter {
        .scale = [scaleAnim] {
            return scaleAnim();
        },
        .child = ZStack {
            .horizontalAlignment = Alignment::Center,
            .verticalAlignment = Alignment::Center,
            .children = flux::children(
                Rectangle {}
                    .fill([boxFillAnim] {
                        return boxFillAnim();
                    })
                    .stroke(boxStroke)
                    .size(boxSize, boxSize)
                    .cornerRadius(CornerRadius {cornerRadius}),
                Icon {
                    .name = iconName,
                    .size = iconSz,
                    .color = [iconColorAnim] {
                        return iconColorAnim();
                    },
                }
            ),
        }
                     .cursor(disabledBinding.evaluate() ? Cursor::Inherit : Cursor::Hand)
                     .focusable(!isDisabled)
                     .onKeyDown(isDisabled ? std::function<void(KeyCode, Modifiers)> {} : std::function<void(KeyCode, Modifiers)> {handleKey})
                     .onTap(isDisabled ? std::function<void()> {} : std::function<void()> {handleToggle}),
    };
}

} // namespace flux
