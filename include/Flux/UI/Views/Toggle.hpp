#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>

#include <functional>

namespace flux {

struct Toggle {
  struct Style {
    float trackWidth = kFloatFromTheme;
    float trackHeight = kFloatFromTheme;
    float thumbInset = kFloatFromTheme;
    float borderWidth = kFloatFromTheme;
    float thumbBorderWidth = kFloatFromTheme;
    Color onColor = kFromTheme;
    Color offColor = kFromTheme;
    Color thumbColor = kFromTheme;
    Color thumbBorderColor = kFromTheme;
    Color borderColor = kFromTheme;
  };

  // ── State ──────────────────────────────────────────────────────────────────

  State<bool> value { };

  // ── Properties ─────────────────────────────────────────────────────────────

  bool disabled { false };
  Style style { };

  // ── Events ─────────────────────────────────────────────────────────────────

  std::function<void(bool)> onChange;


  // ── Component protocol ─────────────────────────────────────────────────────

  Element body() const;
};

} // namespace flux
