#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>

#include <functional>

namespace flux {

struct Checkbox {
  // ── Binding ──────────────────────────────────────────────────────────────

  /// Two-way binding to a boolean signal. Caller owns via useState<bool>().
  /// Tapping flips the value. The indeterminate visual is controlled
  /// separately via the `indeterminate` field — it does not affect `value`.
  State<bool> value{};

  // ── Indeterminate ────────────────────────────────────────────────────────

  /// When true, the checkbox displays a dash instead of a checkmark,
  /// regardless of `value`. Tapping an indeterminate checkbox sets
  /// value = true and the caller should clear `indeterminate` in onChange.
  bool indeterminate = false;

  // ── Appearance ───────────────────────────────────────────────────────────

  /// Box fill when checked/indeterminate. kFromTheme → FluxTheme::colorAccent.
  Color checkedColor = kFromTheme;

  /// Box fill when unchecked. kFromTheme → Colors::transparent.
  Color uncheckedColor = kFromTheme;

  /// Border when unchecked. kFromTheme → FluxTheme::colorBorder.
  Color borderColor = kFromTheme;

  /// Checkmark / dash icon color. kFromTheme → FluxTheme::colorOnAccent.
  Color iconColor = kFromTheme;

  /// Focus ring color. kFromTheme → FluxTheme::colorBorderFocus.
  Color focusColor = kFromTheme;

  // ── Sizing ───────────────────────────────────────────────────────────────

  /// Side length of the box in points. 0 = default (20 pt).
  float boxSize = 0.f;

  /// Corner radius of the box. kFloatFromTheme → FluxTheme::radiusXSmall (4 pt).
  float cornerRadius = kFloatFromTheme;

  // ── Layout ───────────────────────────────────────────────────────────────

  float flexGrow = 0.f;
  float flexShrink = 0.f;
  float minSize = 0.f;

  // ── Behaviour ────────────────────────────────────────────────────────────

  bool disabled = false;

  /// Called after the value flips. Receives the new value.
  std::function<void(bool)> onChange;

  // ── Component protocol ───────────────────────────────────────────────────

  Element body() const;
};

} // namespace flux
