#pragma once

/// \file Flux/UI/Views/Divider.hpp
///
/// Part of the Flux public API.

#include <Flux/UI/Element.hpp>
#include <Flux/UI/Theme.hpp>

namespace flux {

struct Divider : ViewModifiers<Divider> {
    enum class Orientation : std::uint8_t {
        Horizontal,
        Vertical,
    };

    struct Style {
        float thickness = kFloatFromTheme;
        float cornerRadius = kFloatFromTheme;
        Color color = Color::theme();

        bool operator==(Style const& other) const = default;
    };

    Orientation orientation = Orientation::Horizontal;
    Style style {};

    bool operator==(Divider const& other) const {
        return orientation == other.orientation && style == other.style;
    }
    Element body() const;
};

} // namespace flux
