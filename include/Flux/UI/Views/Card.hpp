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
    /// Uniform content inset. `paddingH` / `paddingV` override per-axis when set.
    float padding = kFloatFromTheme;
    /// Horizontal content inset override.
    float paddingH = kFloatFromTheme;
    /// Vertical content inset override.
    float paddingV = kFloatFromTheme;
    /// Card corner radius.
    float cornerRadius = kFloatFromTheme;
    /// Border thickness.
    float borderWidth = kFloatFromTheme;
    /// Card fill color.
    Reactive::Bindable<Color> backgroundColor{Color::theme()};
    /// Card stroke color.
    Reactive::Bindable<Color> borderColor{Color::theme()};
    /// Card shadow style.
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

  /// Card body content.
  Element child;
  /// Optional token overrides.
  Style style {};

  bool operator==(Card const& other) const {
    return child.typeTag() == other.child.typeTag() && style == other.style;
  }

  Element body() const;
};

} // namespace flux
