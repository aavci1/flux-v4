#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>

#include <functional>

namespace flux {

struct Toggle {
  // ── Binding ──────────────────────────────────────────────────────────────

  State<bool> value { };


  // ── Layout ───────────────────────────────────────────────────────────────

  float flexGrow = 0.f;
  float flexShrink = 0.f;
  float minSize = 0.f;


  // ── Behaviour ────────────────────────────────────────────────────────────

  bool disabled = false;

  // ── Style ────────────────────────────────────────────────────────────────

  struct Style {
    float trackWidth = kFloatFromTheme;
    float trackHeight = kFloatFromTheme;
    float thumbInset = kFloatFromTheme;
    float borderWidth = kFloatFromTheme;
    Color onColor = kFromTheme;
    Color offColor = kFromTheme;
    Color thumbColor = kFromTheme;
    Color borderColor = kFromTheme;
  };

  Style style { };


  // ── Events ───────────────────────────────────────────────────────────────

  std::function<void(bool)> onChange;


  // ── Component protocol ───────────────────────────────────────────────────

  Element body() const;
};

} // namespace flux
