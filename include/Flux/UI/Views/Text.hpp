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

  Reactive::Bindable<std::string> text{std::string{}};
  Font font = Font::theme();
  Color color = Color::theme();
  Color selectionColor = Color::theme();
  bool selectable = false;

  HorizontalAlignment horizontalAlignment = HorizontalAlignment::Leading;
  VerticalAlignment verticalAlignment = VerticalAlignment::Top;
  TextWrapping wrapping = TextWrapping::NoWrap;
  int maxLines = 0;
  float firstBaselineOffset = 0.f;

  bool operator==(Text const& other) const {
    bool const sameText = text.isValue() && other.text.isValue() && text.value() == other.text.value();
    return sameText && font == other.font && color == other.color &&
           selectionColor == other.selectionColor && selectable == other.selectable &&
           horizontalAlignment == other.horizontalAlignment &&
           verticalAlignment == other.verticalAlignment && wrapping == other.wrapping &&
           maxLines == other.maxLines &&
           firstBaselineOffset == other.firstBaselineOffset;
  }
};

} // namespace flux
