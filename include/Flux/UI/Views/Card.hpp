#pragma once

/// \file Flux/UI/Views/Card.hpp
///
/// Part of the Flux public API.

#include <Flux/Graphics/Styles.hpp>
#include <Flux/Reactive/Bindable.hpp>
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
    Reactive::Bindable<Color> backgroundColor{Color::theme()};
    Reactive::Bindable<Color> borderColor{Color::theme()};
    Reactive::Bindable<ShadowStyle> shadow{ShadowStyle::none()};

    bool operator==(Style const& other) const {
      bool const sameBackground = backgroundColor.isValue() && other.backgroundColor.isValue() &&
                                  backgroundColor.value() == other.backgroundColor.value();
      bool const sameBorder = borderColor.isValue() && other.borderColor.isValue() &&
                              borderColor.value() == other.borderColor.value();
      bool const sameShadow = shadow.isValue() && other.shadow.isValue() &&
                              shadow.value() == other.shadow.value();
      return padding == other.padding && paddingH == other.paddingH && paddingV == other.paddingV &&
             cornerRadius == other.cornerRadius && borderWidth == other.borderWidth &&
             sameBackground && sameBorder && sameShadow;
    }
  };

  Element child;
  Style style {};

  bool operator==(Card const& other) const {
    return child.typeTag() == other.child.typeTag() && style == other.style;
  }

  Element body() const;
};

} // namespace flux
