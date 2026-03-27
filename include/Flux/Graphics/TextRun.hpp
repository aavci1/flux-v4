#pragma once

#include <Flux/Core/Types.hpp>

#include <cstdint>
#include <vector>

namespace flux {

/// Single-line shaped text with one style (`fontId` / `fontSize` / `color`). Glyph positions are relative to
/// the left edge of the line baseline; `y` is positive downward (canvas space). The shaper merges Core Text
/// runs in display order; style is taken from the first non-empty run (uniform styling is assumed).
struct TextRun {
  Size measuredSize{};
  std::uint32_t fontId = 0;
  float fontSize = 0.f;
  Color color = Colors::black;
  std::vector<std::uint16_t> glyphIds;
  std::vector<Point> positions;
  float ascent = 0.f;
  float descent = 0.f;
};

} // namespace flux
