#pragma once

/// \file Flux/UI/Views/Toggle.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>

#include <functional>

namespace flux {

/// On/off switch control (track + thumb). Binds to \ref value, supports keyboard (Space/Return),
/// pointer, focus ring, and theme-driven motion when \ref Theme::reducedMotion is false.
struct Toggle : ViewModifiers<Toggle> {
  /// Visual tokens; any field may use \c Color::theme() / \c kFloatFromTheme to inherit from \ref Theme.
  struct Style {
    float trackWidth = kFloatFromTheme;
    float trackHeight = kFloatFromTheme;
    float thumbInset = kFloatFromTheme;
    float borderWidth = kFloatFromTheme;
    float thumbBorderWidth = kFloatFromTheme;
    Color onColor = Color::theme();
    Color offColor = Color::theme();
    Color thumbColor = Color::theme();
    Color thumbBorderColor = Color::theme();
    Color borderColor = Color::theme();
  };

  // ── State ──────────────────────────────────────────────────────────────────

  /// Current on/off state; typically from \c useState<bool>() in a parent or owned by this subtree.
  State<bool> value { };

  // ── Properties ─────────────────────────────────────────────────────────────

  /// When true, ignores input and uses disabled styling.
  bool disabled { false };
  Style style { };

  // ── Events ─────────────────────────────────────────────────────────────────

  /// Invoked after the toggle changes \c value (same as mutating \c value from handlers).
  std::function<void(bool)> onChange;


  // ── Component protocol ─────────────────────────────────────────────────────

  /// Builds the animated track/thumb tree. Call only from a composite \c body().
  Element body() const;
};

} // namespace flux
