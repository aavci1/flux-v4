#pragma once

/// \file Flux/UI/Views/Checkbox.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>

#include <functional>

namespace flux {

/// Boolean box with optional indeterminate state and checkmark. Uses \ref Theme checkbox tokens
/// when style fields use sentinels.
struct Checkbox : ViewModifiers<Checkbox> {
  // ── Binding ──────────────────────────────────────────────────────────────

  /// Checked state; bind with \c useState<bool>() or equivalent.
  State<bool> value { };

  // ── Indeterminate ────────────────────────────────────────────────────────

  /// Dash when true; tap sets value true — clear indeterminate in onChange.
  bool indeterminate = false;

  // ── Layout ───────────────────────────────────────────────────────────────
  // Flex: use chained `.flex(...)` on the `Element` from `body()`.

  // ── Behaviour ────────────────────────────────────────────────────────────

  bool disabled = false;

  // ── Style ────────────────────────────────────────────────────────────────

  struct Style {
    float boxSize = kFloatFromTheme;
    float cornerRadius = kFloatFromTheme;
    float borderWidth = kFloatFromTheme;
    Color checkedColor = kColorFromTheme;
    Color uncheckedColor = kColorFromTheme;
    Color checkColor = kColorFromTheme;
    Color borderColor = kColorFromTheme;
  };

  Style style { };


  // ── Events ───────────────────────────────────────────────────────────────

  /// Fires when the user toggles or activates via keyboard; update \c value and clear indeterminate as needed.
  std::function<void(bool)> onChange;


  // ── Component protocol ───────────────────────────────────────────────────

  Element body() const;
};

} // namespace flux
