#pragma once

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>

#include <functional>

namespace flux {

struct Slider {
  // ── Binding ──────────────────────────────────────────────────────────────

  /// Two-way binding to the current value. Caller owns via useState<float>().
  State<float> value{};

  /// Minimum value (left edge of the track). Default = 0.
  float min = 0.f;

  /// Maximum value (right edge of the track). Default = 1.
  float max = 1.f;

  /// Step size. 0 = continuous (no snapping). When > 0, the value
  /// snaps to the nearest multiple of `step` within [min, max].
  float step = 0.f;

  // ── Appearance ───────────────────────────────────────────────────────────

  /// Filled portion of the track (left of thumb). kFromTheme → FluxTheme::colorAccent.
  Color activeColor = kFromTheme;

  /// Unfilled portion of the track (right of thumb). kFromTheme → FluxTheme::colorSurfaceDisabled.
  Color inactiveColor = kFromTheme;

  /// Thumb fill. kFromTheme → Colors::white.
  Color thumbColor = kFromTheme;

  /// Thumb border. kFromTheme → FluxTheme::colorBorder.
  Color thumbBorder = kFromTheme;

  /// Focus ring color. kFromTheme → FluxTheme::colorBorderFocus.
  Color focusColor = kFromTheme;

  // ── Sizing ───────────────────────────────────────────────────────────────

  /// Track height (thickness). Default = 4.
  float trackHeight = 4.f;

  /// Thumb diameter. Default = 20.
  float thumbSize = 20.f;

  /// Total component height. 0 = thumbSize + 8 (vertical padding for
  /// hit area). The track is centered vertically within this height.
  float height = 0.f;

  // ── Layout ───────────────────────────────────────────────────────────────

  float flexGrow = 1.f;
  float flexShrink = 0.f;
  float minSize = 0.f;

  // ── Behaviour ────────────────────────────────────────────────────────────

  bool disabled = false;

  /// Called during drag on every value change (not just on release).
  std::function<void(float)> onChange;

  // ── Component protocol ───────────────────────────────────────────────────

  Element body() const;
};

} // namespace flux
