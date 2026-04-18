#pragma once

/// \file Flux/UI/Views/Text.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <functional>
#include <string>

namespace flux {

/// UTF-8 text in a view box. Size follows layout constraints; use \ref Element modifiers for
/// interaction, padding, frames, backgrounds, and flex.
struct Text : ViewModifiers<Text> {
  static constexpr bool memoizable = true;

  void layout(LayoutContext&) const;
  Size measure(MeasureContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;
  [[nodiscard]] std::uint64_t measureCacheKey() const noexcept;

  std::string text;
  Font font = kFontFromTheme;
  Color color = kColorFromTheme;
  Color selectionColor = kColorFromTheme;
  bool selectable = false;

  HorizontalAlignment horizontalAlignment = HorizontalAlignment::Leading;
  VerticalAlignment verticalAlignment = VerticalAlignment::Top;
  TextWrapping wrapping = TextWrapping::NoWrap;
  int maxLines = 0;
  float firstBaselineOffset = 0.f;
};

} // namespace flux
