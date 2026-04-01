#pragma once

/// \file Flux/Graphics/TextRun.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>

#include <cstdint>
#include <vector>

namespace flux {

/// One Core Text `CTRun`-equivalent: resolved style, shaped glyphs, and metrics. Glyph `positions` are
/// relative to this run’s baseline-left; `y` is positive downward (canvas space).
struct TextRun {
  std::uint32_t fontId = 0;
  float fontSize = 0.f;
  Color color = Colors::black;
  std::vector<std::uint16_t> glyphIds;
  std::vector<Point> positions;
  float ascent = 0.f;
  float descent = 0.f;
  float width = 0.f;
};

} // namespace flux
