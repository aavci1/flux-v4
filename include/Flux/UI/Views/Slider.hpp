#pragma once

/// \file Flux/UI/Views/Slider.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>

#include <functional>

namespace flux {

/// Horizontal slider for a bounded numeric \ref value. Supports pointer drag, scroll wheel where
/// applicable, arrow keys with optional \c step snapping, and focus styling.
struct Slider : ViewModifiers<Slider> {
  struct Style {
    Color activeColor = kColorFromTheme;
    Color inactiveColor = kColorFromTheme;
    Color thumbColor = kColorFromTheme;
    Color thumbBorderColor = kColorFromTheme;
    float trackHeight = kFloatFromTheme;
    float thumbSize = kFloatFromTheme;
  };

  // ── State ──────────────────────────────────────────────────────────────────

  /// Current value in \c [min, max]; use \c useState<float>() or similar.
  State<float> value { };

  // ── Properties ─────────────────────────────────────────────────────────────

  /// Inclusive range endpoints.
  float min { 0.f };
  float max { 1.f };
  /// If \c > 0, pointer and keyboard changes snap to this increment; \c 0 means continuous (pointer)
  /// and small keyboard nudges based on range.
  float step { 0.f };
  bool disabled { false };
  Style style { };

  // ── Events ───────────────────────────────────────────────────────────────

  /// Called whenever \c value changes from user interaction.
  std::function<void(float)> onChange;

  // ── Component protocol ───────────────────────────────────────────────────

  Element body() const;
};

} // namespace flux
