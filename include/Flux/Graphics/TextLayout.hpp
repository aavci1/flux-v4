#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/TextRun.hpp>

#include <vector>

namespace flux {

/// Laid-out text: runs plus placement in layout space. `origin` passed to `Canvas::drawTextLayout` is the layout
/// top-left; each `PlacedRun::origin` is the baseline-left of that run relative to that point (`y` down).
struct TextLayout {
  struct PlacedRun {
    TextRun run{};
    Point origin{}; ///< Baseline-left relative to layout origin (top-left).
  };

  std::vector<PlacedRun> runs;
  Size measuredSize{};
  float firstBaseline = 0.f; ///< Distance from layout top to first line baseline.
  float lastBaseline = 0.f;  ///< Distance from layout top to last line baseline.
};

} // namespace flux
