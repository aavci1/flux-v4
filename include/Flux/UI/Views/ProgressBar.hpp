#pragma once

/// \file Flux/UI/Views/ProgressBar.hpp
///
/// Part of the Flux public API.

#include <Flux/UI/Element.hpp>
#include <Flux/UI/Theme.hpp>

namespace flux {

struct ProgressBar : ViewModifiers<ProgressBar> {
    struct Style {
        Color activeColor = Color::theme();
        Color inactiveColor = Color::theme();
        float trackHeight = kFloatFromTheme;

        bool operator==(Style const& other) const = default;
    };

    float progress = 0.f;
    Style style {};

    bool operator==(ProgressBar const& other) const {
        return progress == other.progress && style == other.style;
    }
    Element body() const;
};

} // namespace flux
