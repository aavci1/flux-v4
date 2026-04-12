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
        Font font = kFontFromTheme;
        float paddingH = kFloatFromTheme;
        float cornerRadius = kFloatFromTheme;
        Color accentColor = kColorFromTheme;
        Color destructiveColor = kColorFromTheme;
    };

    std::string label;
    ButtonVariant variant = ButtonVariant::Primary;
    bool disabled = false;
    Style style {};
    std::function<void()> onTap;
    Element body() const;
};

struct LinkButton : ViewModifiers<LinkButton> {
    struct Style {
        Font font = kFontFromTheme;
        Color color = kColorFromTheme;
    };

    std::string label;
    bool disabled = false;
    Style style {};
    std::function<void()> onTap;

    Element body() const;
};

struct IconButton : ViewModifiers<IconButton> {
    struct Style {
        float size = kFloatFromTheme;
        float weight = kFloatFromTheme;
        Color color = kColorFromTheme;
    };

    IconName icon {};
    bool disabled = false;
    Style style {};
    std::function<void()> onTap;

    Element body() const;
};

} // namespace flux
