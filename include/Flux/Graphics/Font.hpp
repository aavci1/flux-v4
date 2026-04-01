#pragma once

/// \file Flux/Graphics/Font.hpp
///
/// Part of the Flux public API.


#include <string>

namespace flux {

/// Font family, size, and weight. For `AttributedString` runs, an empty `family`, `size <= 0`, or
/// `weight <= 0` inherits from the preceding resolved style; for UI `Text`, use concrete defaults
/// (e.g. size 16, weight 400).
struct Font {
  std::string family;
  float size = 0.f;
  float weight = 0.f;
  bool italic = false;
};

/// Typographic style: font metrics plus line height for vertical rhythm.
/// `lineHeight` is a multiplier (e.g. 1.4 = 140% of font size). Use 0 for the font's natural line height.
struct TextStyle {
  std::string family{};
  float size = 16.f;
  float weight = 400.f;
  bool italic = false;
  float lineHeight = 0.f;

  TextStyle() = default;
  constexpr TextStyle(float sz, float wt, float lh = 0.f) : size(sz), weight(wt), lineHeight(lh) {}

  Font toFont() const;
};

/// Sentinel: inherit `TextStyle` from `FluxTheme`.
inline constexpr TextStyle kStyleFromTheme{-1.f, 0.f};

bool isFromTheme(TextStyle const& s);

TextStyle resolveStyle(TextStyle const& override, TextStyle const& themeValue);

} // namespace flux
