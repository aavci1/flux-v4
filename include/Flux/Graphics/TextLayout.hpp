#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/TextRun.hpp>

#include <cstdint>
#include <vector>

namespace flux {

/// Laid-out text: runs plus placement in layout space. `origin` passed to `Canvas::drawTextLayout` is the layout
/// top-left; each `PlacedRun::origin` is the baseline-left of that run relative to that point (`y` down).
struct TextLayout {
  struct PlacedRun {
    TextRun run{};
    Point origin{}; ///< Baseline-left relative to layout origin (top-left).
    /// Half-open UTF-8 byte range in the source string this run maps to (from Core Text).
    std::uint32_t utf8Begin = 0;
    std::uint32_t utf8End = 0;
    /// Index of the `CTLine` this run came from (stable grouping for hit-testing vs. baseline epsilon).
    std::uint32_t ctLineIndex = 0;
  };

  std::vector<PlacedRun> runs;
  Size measuredSize{};
  float firstBaseline = 0.f; ///< Distance from layout top to first line baseline.
  float lastBaseline = 0.f;  ///< Distance from layout top to last line baseline.
};

/// Recomputes `measuredSize`, `firstBaseline`, and `lastBaseline` from current run geometry.
void recomputeTextLayoutMetrics(TextLayout& layout);

/// Drops runs on lines after the first `maxLines` distinct baselines, then optionally normalizes origins to the
/// bounding box top-left (recommended when returning from a shaper).
void trimTextLayoutToMaxLines(TextLayout& layout, int maxLines, bool normalizeAfter = true);

} // namespace flux
