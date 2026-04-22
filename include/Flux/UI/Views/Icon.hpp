#pragma once

/// \file Flux/UI/Views/Icon.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/IconName.hpp>

namespace flux {

struct Icon : ViewModifiers<Icon> {

    // ── Properties ───────────────────────────────────────────────────────────
    IconName name {};

    /// Icon size in points. Drives both the font size and the component's intrinsic frame.
    /// `kFloatFromTheme` → `Theme::bodyFont.size`.
    float size = kFloatFromTheme;

    /// Icon weight. `kFloatFromTheme` → `Theme::bodyFont.weight`.
    float weight = kFloatFromTheme;

    /// Icon color. `Color::theme()` → `Theme::labelColor`.
    Color color = Color::theme();

    bool operator==(Icon const& other) const {
        return name == other.name && size == other.size && weight == other.weight &&
               color == other.color;
    }

    // ── Component protocol ───────────────────────────────────────────────────
    Element body() const;
};

} // namespace flux
