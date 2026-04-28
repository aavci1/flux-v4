#pragma once

/// \file Flux/UI/Views/ProgressBar.hpp
///
/// Part of the Flux public API.

#include <Flux/UI/Element.hpp>
#include <Flux/UI/Theme.hpp>

namespace flux {

struct ProgressBar : ViewModifiers<ProgressBar> {
    struct Style {
        /// Filled-track color.
        Color activeColor = Color::theme();
        /// Empty-track color.
        Color inactiveColor = Color::theme();
        /// Track thickness.
        float trackHeight = kFloatFromTheme;

        bool operator==(Style const& other) const = default;
    };

    /// Progress in `[0, 1]`.
    float progress = 0.f;
    /// Optional token overrides.
    Style style {};

    bool operator==(ProgressBar const& other) const {
        return progress == other.progress && style == other.style;
    }
    Element body() const;
};

} // namespace flux
