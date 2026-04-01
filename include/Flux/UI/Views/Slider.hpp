#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>

#include <functional>

namespace flux {

struct Slider {
  struct Style {
    Color activeColor = kFromTheme;
    Color inactiveColor = kFromTheme;
    Color thumbColor = kFromTheme;
    Color thumbBorderColor = kFromTheme;
    float trackHeight = kFloatFromTheme;
    float thumbSize = kFloatFromTheme;
  };

  // ── State ──────────────────────────────────────────────────────────────────

  State<float> value { };

  // ── Properties ─────────────────────────────────────────────────────────────

  float min { 0.f };
  float max { 1.f };
  float step { 0.f };
  bool disabled { false };
  Style style { };

  // ── Events ───────────────────────────────────────────────────────────────

  std::function<void(float)> onChange;

  // ── Component protocol ───────────────────────────────────────────────────

  Element body() const;
};

} // namespace flux
