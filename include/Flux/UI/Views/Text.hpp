#pragma once

/// \file Flux/UI/Views/Text.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <functional>
#include <string>

namespace flux {

/// UTF-8 text in a view box. Size follows layout constraints; use \ref Element modifiers for
/// interaction, padding, frames, backgrounds, and flex.
struct Text : ViewModifiers<Text> {
  static constexpr bool memoizable = true;

  void build(BuildContext&) const;
  Size measure(BuildContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  std::string text;
  Font font{ .family = "", .size = 16.f, .weight = 400.f, .italic = false };

  Color color = Colors::black;

  HorizontalAlignment horizontalAlignment = HorizontalAlignment::Leading;
  VerticalAlignment verticalAlignment = VerticalAlignment::Top;
  TextWrapping wrapping = TextWrapping::Wrap;

  float lineHeight = 0.f;
  int maxLines = 0;
  float firstBaselineOffset = 0.f;
};

} // namespace flux
