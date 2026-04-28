#pragma once

/// \file Flux/UI/Views/Text.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Reactive/Bindable.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <functional>
#include <string>

namespace flux {

/// UTF-8 text in a view box. Size follows layout constraints; use \ref Element modifiers for
/// interaction, padding, frames, backgrounds, and flex.
struct Text : ViewModifiers<Text> {
  Size measure(MeasureContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  /// UTF-8 string content.
  Reactive::Bindable<std::string> text{std::string{}};
  /// Text font.
  Font font = Font::theme();
  /// Resolved text color.
  Reactive::Bindable<Color> color{Color::theme()};
  /// Highlight fill used for text selection when `selectable` is enabled.
  Color selectionColor = Color::theme();
  /// Enables selection interaction on desktop platforms.
  bool selectable = false;

  /// Horizontal placement of laid-out lines inside the view box.
  HorizontalAlignment horizontalAlignment = HorizontalAlignment::Leading;
  /// Vertical placement of the text block inside the view box.
  VerticalAlignment verticalAlignment = VerticalAlignment::Top;
  /// Wrapping strategy for long lines.
  TextWrapping wrapping = TextWrapping::NoWrap;
  /// Maximum number of laid-out lines. `0` means unlimited.
  int maxLines = 0;
  /// Additional first-baseline inset, mainly for aligning mixed text treatments.
  float firstBaselineOffset = 0.f;

  bool operator==(Text const& other) const {
    bool const sameText = text.isValue() && other.text.isValue() && text.value() == other.text.value();
    bool const sameColor = color.isValue() && other.color.isValue() && color.value() == other.color.value();
    return sameText && font == other.font && sameColor &&
           selectionColor == other.selectionColor && selectable == other.selectable &&
           horizontalAlignment == other.horizontalAlignment &&
           verticalAlignment == other.verticalAlignment && wrapping == other.wrapping &&
           maxLines == other.maxLines &&
           firstBaselineOffset == other.firstBaselineOffset;
  }
};

} // namespace flux
