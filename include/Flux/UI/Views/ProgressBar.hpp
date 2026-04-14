#pragma once

/// \file Flux/UI/Views/ProgressBar.hpp
///
/// Part of the Flux public API.

#include <Flux/UI/Element.hpp>
#include <Flux/UI/Theme.hpp>

namespace flux {

struct ProgressBar : ViewModifiers<ProgressBar> {
    struct Style {
        Color activeColor = kColorFromTheme;
        Color inactiveColor = kColorFromTheme;
        float trackHeight = kFloatFromTheme;
    };

    float progress = 0.f;
    Style style {};

    Element body() const;
};

} // namespace flux
