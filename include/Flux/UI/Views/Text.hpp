#pragma once

/// \file Flux/UI/Views/Text.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/AttributedString.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace flux {

/// UTF-8 text in a view box. Size follows layout constraints; use \ref Element modifiers for
/// interaction, padding, frames, backgrounds, and flex.
///
/// Provide either the plain fields (\c text, \c style, \c color) or \c attributed for rich text.
/// When \c attributed is set, its UTF-8 and runs are used for layout and rendering; \c text is
/// ignored for display (line metrics and alignment still come from \c style and the other \c Text
/// fields). Use \c attributed for document-style output aligned with \ref TextSystem and future
/// editor views that produce \ref AttributedString.
struct Text : ViewModifiers<Text> {
  static constexpr bool memoizable = true;

  void layout(LayoutContext&) const;
  void renderFromLayout(RenderContext&, LayoutNode const&) const;
  Size measure(LayoutContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  /// Plain UTF-8 string with uniform \c style and \c color. Ignored for display when \c attributed
  /// is set.
  std::string text;
  TextStyle style{};

  Color color = Colors::black;

  /// When set, rich layout uses this string; \c text is not shown.
  std::optional<AttributedString> attributed;

  HorizontalAlignment horizontalAlignment = HorizontalAlignment::Leading;
  VerticalAlignment verticalAlignment = VerticalAlignment::Top;
  TextWrapping wrapping = TextWrapping::Wrap;

  int maxLines = 0;
  float firstBaselineOffset = 0.f;
};

} // namespace flux
