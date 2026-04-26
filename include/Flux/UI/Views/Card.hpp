#pragma once

/// \file Flux/UI/Views/Card.hpp
///
/// Part of the Flux public API.

#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Theme.hpp>

namespace flux {

/// Surface container for card-like content: padded elevated background, optional border, radius, and shadow.
struct Card : ViewModifiers<Card> {
  struct Style {
    float padding = kFloatFromTheme;
    float paddingH = kFloatFromTheme;
    float paddingV = kFloatFromTheme;
    float cornerRadius = kFloatFromTheme;
    float borderWidth = kFloatFromTheme;
    Color backgroundColor = Color::theme();
    Color borderColor = Color::theme();
    ShadowStyle shadow = ShadowStyle::none();

    bool operator==(Style const& other) const = default;
  };

  Element child;
  Style style {};

  bool operator==(Card const& other) const {
    return child.typeTag() == other.child.typeTag() && style == other.style;
  }

  Element body() const;
};

} // namespace flux
