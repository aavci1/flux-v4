#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>

#include <functional>

namespace flux {

struct Toggle {
  // ── Binding ──────────────────────────────────────────────────────────────

  /// Two-way binding to a boolean signal. Caller owns via useState<bool>().
  State<bool> value{};

  // ── Appearance ───────────────────────────────────────────────────────────

  /// Track color when ON.  kFromTheme → FluxTheme::colorAccent.
  Color onColor = kFromTheme;

  /// Track color when OFF. kFromTheme → FluxTheme::colorSurfaceDisabled.
  Color offColor = kFromTheme;

  /// Thumb color (both states). kFromTheme → Colors::white.
  Color thumbColor = kFromTheme;

  /// Track border when OFF. kFromTheme → FluxTheme::colorBorder.
  Color borderColor = kFromTheme;

  /// Focus ring color. kFromTheme → FluxTheme::colorBorderFocus.
  Color focusColor = kFromTheme;

  // ── Sizing ───────────────────────────────────────────────────────────────

  /// Track dimensions.  0 = defaults (44 × 26 pt — macOS convention).
  float trackWidth = 0.f;
  float trackHeight = 0.f;

  /// Thumb inset from track edge in the OFF position (pt).
  /// The same inset is used on all sides. Default = 3.
  float thumbInset = 3.f;

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
