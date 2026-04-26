#pragma once

/// \file Flux/UI/Views/Icon.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Reactive/Bindable.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/IconName.hpp>

namespace flux {

struct Icon : ViewModifiers<Icon> {

    // ── Properties ───────────────────────────────────────────────────────────
    Reactive::Bindable<IconName> name {};

    /// Icon size in points. Drives both the font size and the component's intrinsic frame.
    /// `kFloatFromTheme` → `Theme::bodyFont.size`.
    float size = kFloatFromTheme;

    /// Icon weight. `kFloatFromTheme` → `Theme::bodyFont.weight`.
    float weight = kFloatFromTheme;

    /// Icon color. `Color::theme()` → `Theme::labelColor`.
    Color color = Color::theme();

    bool operator==(Icon const& other) const {
        bool const sameName = name.isValue() && other.name.isValue() && name.value() == other.name.value();
        return sameName && size == other.size && weight == other.weight &&
               color == other.color;
    }

    // ── Component protocol ───────────────────────────────────────────────────
    Element body() const;
};

} // namespace flux
