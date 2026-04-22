#pragma once

/// \file Flux/UI/Views/Badge.hpp
///
/// Part of the Flux public API.

#include <Flux/Graphics/Font.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Theme.hpp>

#include <string>

namespace flux {

struct Badge : ViewModifiers<Badge> {
    struct Style {
        Font font = Font::theme();
        float paddingH = kFloatFromTheme;
        float paddingV = kFloatFromTheme;
        float cornerRadius = kFloatFromTheme;
        Color foregroundColor = Color::theme();
        Color backgroundColor = Color::theme();

        bool operator==(Style const& other) const = default;
    };

    std::string label;
    Style style {};

    bool operator==(Badge const& other) const {
        return label == other.label && style == other.style;
    }
    Element body() const;
};

} // namespace flux
