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

    std::string label;
    ButtonVariant variant = ButtonVariant::Primary;
    bool disabled = false;
    Style style {};
    std::function<void()> onTap;

    bool operator==(Button const& other) const {
        return label == other.label && variant == other.variant && disabled == other.disabled &&
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

    std::string label;
    bool disabled = false;
    Style style {};
    std::function<void()> onTap;

    bool operator==(LinkButton const& other) const {
        return label == other.label && disabled == other.disabled && style == other.style;
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
    bool disabled = false;
    Style style {};
    std::function<void()> onTap;

    bool operator==(IconButton const& other) const {
        return icon == other.icon && disabled == other.disabled && style == other.style;
    }

    Element body() const;
};

} // namespace flux
