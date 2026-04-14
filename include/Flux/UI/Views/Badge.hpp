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
        Font font = kFontFromTheme;
        float paddingH = kFloatFromTheme;
        float paddingV = kFloatFromTheme;
        float cornerRadius = kFloatFromTheme;
        Color foregroundColor = kColorFromTheme;
        Color backgroundColor = kColorFromTheme;
    };

    std::string label;
    Style style {};

    Element body() const;
};

} // namespace flux
