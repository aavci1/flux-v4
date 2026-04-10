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
    /// `kFloatFromTheme` → `Theme::typeBody.size`.
    float size = kFloatFromTheme;

    /// Icon weight. `kFloatFromTheme` → `Theme::typeBody.weight`.
    float weight = kFloatFromTheme;

    /// Icon color. `kColorFromTheme` → `Theme::colorTextPrimary`.
    Color color = kColorFromTheme;

    // ── Component protocol ───────────────────────────────────────────────────
    Element body() const;
};

} // namespace flux
