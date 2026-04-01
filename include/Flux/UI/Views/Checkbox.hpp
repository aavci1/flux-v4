#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>

#include <functional>

namespace flux {

struct Checkbox {
  // ── Binding ──────────────────────────────────────────────────────────────

  State<bool> value { };

  // ── Indeterminate ────────────────────────────────────────────────────────

  /// Dash when true; tap sets value true — clear indeterminate in onChange.
  bool indeterminate = false;

  // ── Layout ───────────────────────────────────────────────────────────────

  float flexGrow = 0.f;
  float flexShrink = 0.f;
  float minSize = 0.f;

  // ── Behaviour ────────────────────────────────────────────────────────────

  bool disabled = false;

  // ── Style ────────────────────────────────────────────────────────────────

  struct Style {
    float boxSize = kFloatFromTheme;
    float cornerRadius = kFloatFromTheme;
    float borderWidth = kFloatFromTheme;
    Color checkedColor = kFromTheme;
    Color uncheckedColor = kFromTheme;
    Color checkColor = kFromTheme;
    Color borderColor = kFromTheme;
  };

  Style style { };


  // ── Events ───────────────────────────────────────────────────────────────

  std::function<void(bool)> onChange;


  // ── Component protocol ───────────────────────────────────────────────────

  Element body() const;
};

} // namespace flux
