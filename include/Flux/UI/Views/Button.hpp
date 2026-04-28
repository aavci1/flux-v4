#pragma once

/// \file Flux/UI/Views/Button.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/IconName.hpp>
#include <Flux/UI/Theme.hpp>

#include <functional>
#include <string>

namespace flux {

enum class ButtonVariant : std::uint8_t {
    Primary,
    Secondary,
    Destructive,
    Ghost,
};

struct Button : ViewModifiers<Button> {
    struct Style {
        Font font = Font::theme();
        float paddingH = kFloatFromTheme;
        float paddingV = kFloatFromTheme;
        float cornerRadius = kFloatFromTheme;
        Color accentColor = Color::theme();
        Color destructiveColor = Color::theme();

        bool operator==(Style const& other) const = default;
    };

    Reactive::Bindable<std::string> label{std::string{}};
    ButtonVariant variant = ButtonVariant::Primary;
    Reactive::Bindable<bool> disabled{false};
    Style style {};
    std::function<void()> onTap;

    bool operator==(Button const& other) const {
        bool const sameLabel = label.isValue() && other.label.isValue() && label.value() == other.label.value();
        bool const sameDisabled = disabled.isValue() && other.disabled.isValue() &&
                                  disabled.value() == other.disabled.value();
        return sameLabel && variant == other.variant && sameDisabled &&
               style == other.style;
    }

    Element body() const;
};

struct LinkButton : ViewModifiers<LinkButton> {
    struct Style {
        Font font = Font::theme();
        Color color = Color::theme();

        bool operator==(Style const& other) const = default;
    };

    Reactive::Bindable<std::string> label{std::string{}};
    Reactive::Bindable<bool> disabled{false};
    Style style {};
    std::function<void()> onTap;

    bool operator==(LinkButton const& other) const {
        bool const sameLabel = label.isValue() && other.label.isValue() && label.value() == other.label.value();
        bool const sameDisabled = disabled.isValue() && other.disabled.isValue() &&
                                  disabled.value() == other.disabled.value();
        return sameLabel && sameDisabled && style == other.style;
    }

    Element body() const;
};

struct IconButton : ViewModifiers<IconButton> {
    struct Style {
        float size = kFloatFromTheme;
        float weight = kFloatFromTheme;
        Color color = Color::theme();

        bool operator==(Style const& other) const = default;
    };

    IconName icon {};
    Reactive::Bindable<bool> disabled{false};
    Style style {};
    std::function<void()> onTap;

    bool operator==(IconButton const& other) const {
        bool const sameDisabled = disabled.isValue() && other.disabled.isValue() &&
                                  disabled.value() == other.disabled.value();
        return icon == other.icon && sameDisabled && style == other.style;
    }

    Element body() const;
};

} // namespace flux
