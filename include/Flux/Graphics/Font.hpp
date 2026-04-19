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

  static Font theme() { return semantic(1.f); }
  static Font largeTitle() { return semantic(2.f); }
  static Font title() { return semantic(3.f); }
  static Font title2() { return semantic(4.f); }
  static Font title3() { return semantic(5.f); }
  static Font headline() { return semantic(6.f); }
  static Font subheadline() { return semantic(7.f); }
  static Font body() { return semantic(8.f); }
  static Font callout() { return semantic(9.f); }
  static Font footnote() { return semantic(10.f); }
  static Font caption() { return semantic(11.f); }
  static Font caption2() { return semantic(12.f); }
  static Font monospacedBody() { return semantic(13.f); }

  int semanticToken() const { return size < 0.f && family.empty() ? static_cast<int>(-size) : 0; }
  bool isSemantic() const { return semanticToken() != 0; }

  bool operator==(Font const& other) const = default;

private:
  static Font semantic(float token) {
    Font font;
    font.size = -token;
    return font;
  }
};

} // namespace flux
