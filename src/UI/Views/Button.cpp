#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/UI/Theme.hpp>
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
    return Color {lerp(c.r, w.r, t), lerp(c.g, w.g, t), lerp(c.b, w.b, t), c.a};
}

Color darken(Color c, float t) {
    Color const b = Colors::black;
    return Color {lerp(c.r, b.r, t), lerp(c.g, b.g, t), lerp(c.b, b.b, t), c.a};
}

struct ButtonColors {
    Color fill {};
    Color fillHover {};
    Color fillPress {};
    Color label {};
    Color border {};
    Color focusRing {};
    ShadowStyle shadow = ShadowStyle::none();
};

Button::Style resolveStyle(Button::Style const &style, Theme const &theme) {
    return Button::Style {
        .font = resolveFont(style.font, theme.fontLabel),
        .paddingH = resolveFloat(style.paddingH, theme.space4),
        .cornerRadius = resolveFloat(style.cornerRadius, theme.radiusMedium * 0.5f),
        .accentColor = resolveColor(style.accentColor, theme.colorAccent),
        .destructiveColor = resolveColor(style.destructiveColor, theme.colorDanger),
    };
}

LinkButton::Style resolveStyle(LinkButton::Style const &style, Theme const &theme) {
    return LinkButton::Style {
        .font = resolveFont(style.font, theme.fontBody),
        .color = resolveColor(style.color, theme.colorAccent),
    };
}

ButtonColors deriveColors(ButtonVariant variant, Color accent, Color destructive, Color onAccent,
                          Color onDanger, Theme const &theme) {
    switch (variant) {
    case ButtonVariant::Primary:
        return {
            .fill = accent,
            .fillHover = lighten(accent, 0.08f),
            .fillPress = darken(accent, 0.08f),
            .label = onAccent,
            .border = Colors::transparent,
            .focusRing = theme.colorBorderFocus,
            .shadow = ShadowStyle {.radius = theme.shadowRadiusControl, .offset = {0.f, theme.shadowOffsetYControl}, .color = theme.shadowColor},
        };
    case ButtonVariant::Secondary:
        return {
            .fill = theme.colorSurfaceOverlay,
            .fillHover = theme.colorSurfaceHover,
            .fillPress = theme.colorSurfaceDisabled,
            .label = theme.colorTextPrimary,
            .border = theme.colorBorder,
            .focusRing = theme.colorBorderFocus,
            .shadow = ShadowStyle::none(),
        };
    case ButtonVariant::Destructive:
        return {
            .fill = destructive,
            .fillHover = lighten(destructive, 0.08f),
            .fillPress = darken(destructive, 0.08f),
            .label = onDanger,
            .border = Colors::transparent,
            .focusRing = theme.colorBorderFocus,
            .shadow = ShadowStyle {.radius = theme.shadowRadiusControl, .offset = {0.f, theme.shadowOffsetYControl}, .color = theme.shadowColor},
        };
    case ButtonVariant::Ghost:
        return {
            .fill = Colors::transparent,
            .fillHover = theme.colorAccentSubtle,
            .fillPress = Color {theme.colorAccentSubtle.r, theme.colorAccentSubtle.g, theme.colorAccentSubtle.b, std::min(theme.colorAccentSubtle.a + 0.08f, 1.f)},
            .label = accent,
            .border = Colors::transparent,
            .focusRing = theme.colorBorderFocus,
            .shadow = ShadowStyle::none(),
        };
    }
    return {};
}

} // namespace

Element Button::body() const {
    Theme const &theme = useEnvironment<Theme>();
    auto [fontResolved, paddingResolved, radiusResolved, accent, destructive] = resolveStyle(style, theme);
    bool const isDisabled = disabled;
    ButtonColors const colors = deriveColors(variant, accent, destructive, theme.colorOnAccent, theme.colorOnDanger, theme);

    Transition const trInstant = Transition::instant();
    Transition const trFast = theme.reducedMotion ? trInstant : Transition::ease(theme.durationFast);
    Transition const trMed = theme.reducedMotion ? trInstant : Transition::ease(theme.durationMedium);
    Transition const tr = isDisabled ? trInstant : trMed;

    bool const hovered = useHover();
    bool const pressed = usePress();
    bool const focused = useFocus();

    Color const fillTarget = isDisabled ? theme.colorSurfaceDisabled :
                             pressed    ? colors.fillPress :
                             hovered    ? colors.fillHover :
                                          colors.fill;
    Color const labelTarget = isDisabled ? theme.colorTextDisabled : colors.label;

    auto fillAnim = useAnimated<Color>(colors.fill);
    if (*fillAnim != fillTarget) {
        fillAnim.set(fillTarget, tr);
    }

    auto labelAnim = useAnimated<Color>(colors.label);
    if (*labelAnim != labelTarget) {
        labelAnim.set(labelTarget, tr);
    }

    auto scaleAnim = useAnimated<float>(1.f);
    float const scaleTarget = (pressed && !isDisabled) ? 0.97f : 1.f;
    if (std::abs(*scaleAnim - scaleTarget) > 0.001f) {
        scaleAnim.set(scaleTarget, trFast);
    }

    ShadowStyle shadow = ShadowStyle::none();
    if (!isDisabled) {
        if (pressed) {
            shadow = ShadowStyle {.radius = theme.shadowRadiusControl + 2.f, .offset = {0.f, theme.shadowOffsetYControl + 1.f}, .color = Color {theme.shadowColor.r, theme.shadowColor.g, theme.shadowColor.b, std::min(theme.shadowColor.a + 0.08f, 1.f)}};
        } else if (hovered) {
            shadow = colors.shadow.isNone() ? ShadowStyle {.radius = theme.shadowRadiusControl * 0.8f, .offset = {0.f, theme.shadowOffsetYControl + 0.5f}, .color = Color {theme.shadowColor.r, theme.shadowColor.g, theme.shadowColor.b, theme.shadowColor.a * 0.7f}} : colors.shadow;
        } else {
            shadow = colors.shadow;
        }
    }

    auto handleTap = [onTap = onTap, isDisabled]() {
        if (isDisabled) {
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

    bool const showFocusRing = !isDisabled && focused;
    CornerRadius const cr {radiusResolved};

    StrokeStyle const stroke = showFocusRing                            ? StrokeStyle::solid(colors.focusRing, 2.f) :
                               (!isDisabled && colors.border.a > 0.01f) ? StrokeStyle::solid(colors.border, 1.f) :
                                                                          StrokeStyle::none();

    return ScaleAroundCenter {
        .scale = *scaleAnim,
        .child = Text {
            .text = label,
            .font = fontResolved,
            .color = *labelAnim,
            .horizontalAlignment = HorizontalAlignment::Center,
            .verticalAlignment = VerticalAlignment::Center,
        }
                     .fill(FillStyle::solid(*fillAnim))
                     .stroke(stroke)
                     .cornerRadius(cr)
                     .shadow(shadow)
                     .padding(paddingResolved)
                     .cursor(isDisabled ? Cursor::Inherit : Cursor::Hand)
                     .focusable(!isDisabled)
                     .onKeyDown(isDisabled ? std::function<void(KeyCode, Modifiers)> {} : std::function<void(KeyCode, Modifiers)> {handleKey})
                     .onTap(isDisabled ? std::function<void()> {} : std::function<void()> {handleTap})
    };
}

Element LinkButton::body() const {
    Theme const &theme = useEnvironment<Theme>();
    auto [fontResolved, accentResolved] = resolveStyle(style, theme);
    bool const isDisabled = disabled;
    bool const hovered = useHover();
    bool const pressed = usePress();
    bool const focused = useFocus();
    bool const keyboardFocused = useKeyboardFocus();

    Transition const trMed = theme.reducedMotion ? Transition::instant() : Transition::ease(theme.durationMedium);

    auto labelAnim = useAnimated<Color>(accentResolved);
    {
        Color const target =
            isDisabled ? theme.colorTextDisabled : pressed ? darken(accentResolved, 0.12f) :
                                               hovered     ? lighten(accentResolved, 0.12f) :
                                                             accentResolved;
        if (*labelAnim != target) {
            labelAnim.set(target, trMed);
        }
    }

    auto handleTap = [onTap = onTap, isDisabled]() {
        if (isDisabled) {
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

    StrokeStyle focusStroke = StrokeStyle::none();
    if (!isDisabled && focused && keyboardFocused) {
        focusStroke = StrokeStyle::solid(theme.colorBorderFocus, 2.f);
    }

    return Text {
        .text = label,
        .font = fontResolved,
        .color = *labelAnim,
        .horizontalAlignment = HorizontalAlignment::Leading,
        .verticalAlignment = VerticalAlignment::Center,
    }
        .fill(FillStyle::none())
        .stroke(focusStroke)
        .cornerRadius(CornerRadius {theme.radiusXSmall})
        .padding(0.f, 3.f, 0.f, 3.f)
        .cursor(isDisabled ? Cursor::Inherit : Cursor::Hand)
        .focusable(!isDisabled)
        .onKeyDown(isDisabled ? std::function<void(KeyCode, Modifiers)> {} : std::function<void(KeyCode, Modifiers)> {handleKey})
        .onTap(isDisabled ? std::function<void()> {} : std::function<void()> {handleTap});
}

} // namespace flux
