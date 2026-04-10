#pragma once

/// \file Flux/UI/Views/Icon.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/IconName.hpp>

namespace flux {

struct Icon : ViewModifiers<Icon> {

  // ── Required ────────────────────────────────────────────────────────
  IconName name {};

  // ── Appearance ──────────────────────────────────────────────────────

  /// Icon size in points. Drives both the font size and the component's intrinsic frame.
  /// `kFloatFromTheme` → `Theme::typeBody.size` (24 pt).
  float size = kFloatFromTheme;

  /// 0 = use theme body weight (`Theme::typeBody`).
  float weight = kFloatFromTheme;

  /// Icon colour. `kColorFromTheme` → `Theme::colorTextPrimary`.
  Color color = kColorFromTheme;

  // ── Component protocol ────────────────────────────────────────────────
  Element body() const;
};

} // namespace flux
