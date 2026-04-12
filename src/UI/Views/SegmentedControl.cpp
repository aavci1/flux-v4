#include <Flux/UI/Views/SegmentedControl.hpp>

#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

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

struct ResolvedStyle {
    Font font {};
    float paddingH = 0.f;
    float paddingV = 0.f;
    float cornerRadius = 0.f;
    Color accentColor {};
    Color trackColor {};
    Color borderColor {};
};

ResolvedStyle resolveStyle(SegmentedControl::Style const &style, Theme const &theme) {
    return ResolvedStyle {
        .font = resolveFont(style.font, theme.fontLabel),
        .paddingH = resolveFloat(style.paddingH, theme.space4),
        .paddingV = resolveFloat(style.paddingV, theme.space2),
        .cornerRadius = resolveFloat(style.cornerRadius, theme.radiusMedium),
        .accentColor = resolveColor(style.accentColor, theme.colorAccent),
        .trackColor = resolveColor(style.trackColor, theme.colorSurfaceField),
        .borderColor = resolveColor(style.borderColor, theme.colorBorderSubtle),
    };
}

int clampIndex(int index, std::size_t count) {
    if (count == 0) {
        return -1;
    }
    return std::clamp(index, 0, static_cast<int>(count - 1));
}

struct SegmentedControlItem : ViewModifiers<SegmentedControlItem> {
    SegmentedControlOption option;
    bool selected = false;
    bool disabled = false;
    ResolvedStyle style {};
    Theme theme {};
    std::function<void()> onTap;

    Element body() const {
        bool const isDisabled = disabled || option.disabled;
        bool const hovered = useHover();
        bool const pressed = usePress();
        bool const focused = useFocus();
        bool const keyboardFocused = useKeyboardFocus();

        Color const selectedFill = style.accentColor;
        Color const selectedHoverFill = lighten(style.accentColor, 0.08f);
        Color const selectedPressFill = darken(style.accentColor, 0.08f);
        Color const idleFill = Colors::transparent;
        Color const hoverFill = theme.colorSurfaceHover;
        Color const pressFill = theme.colorSurfaceRowHover;
        Color const fill = isDisabled ? Colors::transparent
                           : selected ? (pressed ? selectedPressFill : hovered ? selectedHoverFill : selectedFill)
                                      : (pressed ? pressFill : hovered ? hoverFill : idleFill);
        Color const labelColor = isDisabled ? theme.colorTextDisabled
                                 : selected ? theme.colorOnAccent
                                            : theme.colorTextSecondary;
        StrokeStyle const stroke =
            !isDisabled && focused && keyboardFocused
                ? StrokeStyle::solid(theme.colorBorderFocus, 2.f)
                : StrokeStyle::none();

        auto handleTap = [isDisabled, onTap = onTap]() {
            if (!isDisabled && onTap) {
                onTap();
            }
        };
        auto handleKey = [handleTap](KeyCode key, Modifiers) {
            if (key == keys::Return || key == keys::Space) {
                handleTap();
            }
        };

        return ZStack {
            .horizontalAlignment = Alignment::Center,
            .verticalAlignment = Alignment::Center,
            .children = children(
                Text {
                    .text = option.label,
                    .font = style.font,
                    .color = labelColor,
                    .horizontalAlignment = HorizontalAlignment::Center,
                    .verticalAlignment = VerticalAlignment::Center,
                }
            ),
        }
            .padding(style.paddingV, style.paddingH, style.paddingV, style.paddingH)
            .fill(FillStyle::solid(fill))
            .stroke(stroke)
            .cornerRadius(CornerRadius {std::max(0.f, style.cornerRadius - 2.f)})
            .cursor(isDisabled ? Cursor::Arrow : Cursor::Hand)
            .focusable(!isDisabled)
            .onKeyDown(isDisabled ? std::function<void(KeyCode, Modifiers)> {} :
                                    std::function<void(KeyCode, Modifiers)> {handleKey})
            .onTap(isDisabled ? std::function<void()> {} : std::function<void()> {handleTap});
    }
};

} // namespace

Element SegmentedControl::body() const {
    Theme const &theme = useEnvironment<Theme>();
    ResolvedStyle const resolved = resolveStyle(style, theme);
    LayoutConstraints const *layoutConstraints = useLayoutConstraints();
    State<int> const selection = selectedIndex.signal ? selectedIndex : useState<int>(0);
    int const currentIndex = clampIndex(*selection, options.size());

    auto commitSelection = [selection, onChange = onChange](int index) {
        selection = index;
        if (onChange) {
            onChange(index);
        }
    };
    auto handleKey = [commitSelection, options = options, currentIndex](KeyCode key, Modifiers) {
        if (options.empty()) {
            return;
        }

        int nextIndex = currentIndex;
        if (key == keys::LeftArrow) {
            nextIndex = std::max(0, currentIndex - 1);
        } else if (key == keys::RightArrow) {
            nextIndex = std::min(static_cast<int>(options.size() - 1), currentIndex + 1);
        } else {
            return;
        }

        while (nextIndex >= 0 && static_cast<std::size_t>(nextIndex) < options.size() &&
               options[static_cast<std::size_t>(nextIndex)].disabled) {
            nextIndex += key == keys::LeftArrow ? -1 : 1;
        }
        if (nextIndex >= 0 && static_cast<std::size_t>(nextIndex) < options.size()) {
            commitSelection(nextIndex);
        }
    };

    std::vector<Element> items;
    items.reserve(options.size());
    for (std::size_t i = 0; i < options.size(); ++i) {
        items.push_back(SegmentedControlItem {
            .option = options[i],
            .selected = static_cast<int>(i) == currentIndex,
            .disabled = disabled,
            .style = resolved,
            .theme = theme,
            .onTap = [commitSelection, index = static_cast<int>(i)] {
                commitSelection(index);
            },
        }.flex(1.f, 1.f, 0.f));
    }

    Element root = HStack {
        .spacing = theme.space1,
        .alignment = Alignment::Stretch,
        .children = std::move(items),
    }
        .padding(2.f)
        .fill(FillStyle::solid(disabled ? theme.colorSurfaceDisabled : resolved.trackColor))
        .stroke(StrokeStyle::solid(resolved.borderColor, 1.f))
        .cornerRadius(CornerRadius {resolved.cornerRadius})
        .focusable(!disabled)
        .onKeyDown(disabled ? std::function<void(KeyCode, Modifiers)> {} :
                              std::function<void(KeyCode, Modifiers)> {handleKey});

    if (layoutConstraints && std::isfinite(layoutConstraints->maxWidth) && layoutConstraints->maxWidth > 0.f) {
        root = std::move(root).width(layoutConstraints->maxWidth);
    }
    return root;
}

} // namespace flux
