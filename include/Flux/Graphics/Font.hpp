#pragma once

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

} // namespace flux
