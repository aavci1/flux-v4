#include <Flux/Graphics/TextSystem.hpp>

#include "Graphics/TextSystemPrivate.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <vector>

namespace flux {

void recomputeTextLayoutMetrics(TextLayout& L) {
  if (L.runs.empty()) {
    L.measuredSize = {};
    L.firstBaseline = 0.f;
    L.lastBaseline = 0.f;
    L.lines.clear();
    return;
  }

  float minX = std::numeric_limits<float>::infinity();
  float maxX = -std::numeric_limits<float>::infinity();
  float minTop = std::numeric_limits<float>::infinity();
  float maxBot = -std::numeric_limits<float>::infinity();
  float minBaselineY = std::numeric_limits<float>::infinity();
  float maxBaselineY = -std::numeric_limits<float>::infinity();

  for (auto const& pr : L.runs) {
    TextRun const& r = pr.run;
    minX = std::min(minX, pr.origin.x);
    maxX = std::max(maxX, pr.origin.x + r.width);
    minTop = std::min(minTop, pr.origin.y - r.ascent);
    maxBot = std::max(maxBot, pr.origin.y + r.descent);
    minBaselineY = std::min(minBaselineY, pr.origin.y);
    maxBaselineY = std::max(maxBaselineY, pr.origin.y);
  }

  L.measuredSize.width = std::max(0.f, maxX - minX);
  L.measuredSize.height = std::max(0.f, maxBot - minTop);
  L.firstBaseline = minBaselineY - minTop;
  L.lastBaseline = maxBaselineY - minTop;
}

namespace {

constexpr float kLineEps = 0.5f;

bool nearLine(float a, float b) { return std::abs(a - b) < kLineEps; }

void applyHorizontalPerLine(TextLayout& layout, Rect const& box, HorizontalAlignment horizontalAlignment) {
  if (layout.runs.empty()) {
    return;
  }
  if (horizontalAlignment == HorizontalAlignment::Leading) {
    return;
  }

  std::vector<float> baselines;
  baselines.reserve(layout.runs.size());
  for (auto const& pr : layout.runs) {
    float const y = pr.origin.y;
    bool found = false;
    for (float b : baselines) {
      if (nearLine(b, y)) {
        found = true;
        break;
      }
    }
    if (!found) {
      baselines.push_back(y);
    }
  }

  for (float ly : baselines) {
    float minL = std::numeric_limits<float>::infinity();
    float maxR = -std::numeric_limits<float>::infinity();
    for (auto const& pr : layout.runs) {
      if (!nearLine(pr.origin.y, ly)) {
        continue;
      }
      minL = std::min(minL, pr.origin.x);
      maxR = std::max(maxR, pr.origin.x + pr.run.width);
    }
    if (!(minL < std::numeric_limits<float>::infinity())) {
      continue;
    }
    float const lineWidth = maxR - minL;
    float lineDx = 0.f;
    if (horizontalAlignment == HorizontalAlignment::Center) {
      lineDx = (box.width - lineWidth) * 0.5f - minL;
    } else if (horizontalAlignment == HorizontalAlignment::Trailing) {
      lineDx = box.width - lineWidth - minL;
    }
    for (auto& pr : layout.runs) {
      if (nearLine(pr.origin.y, ly)) {
        pr.origin.x += lineDx;
      }
    }
    for (auto& lr : layout.lines) {
      if (nearLine(lr.baseline, ly)) {
        lr.lineMinX += lineDx;
      }
    }
  }
  recomputeTextLayoutMetrics(layout);
}

} // namespace

namespace detail {

void normalizeOriginsToTopLeft(TextLayout& L) {
  if (L.runs.empty()) {
    return;
  }
  float minX = std::numeric_limits<float>::infinity();
  float minTop = std::numeric_limits<float>::infinity();
  for (auto const& pr : L.runs) {
    TextRun const& r = pr.run;
    minX = std::min(minX, pr.origin.x);
    minTop = std::min(minTop, pr.origin.y - r.ascent);
  }
  for (auto& pr : L.runs) {
    pr.origin.x -= minX;
    pr.origin.y -= minTop;
  }
  for (auto& lr : L.lines) {
    lr.lineMinX -= minX;
    lr.top -= minTop;
    lr.bottom -= minTop;
    lr.baseline -= minTop;
  }
  recomputeTextLayoutMetrics(L);
}

void applyBoxOptions(TextLayout& layout, Rect const& box, TextLayoutOptions const& options) {
  if (layout.runs.empty()) {
    return;
  }

  normalizeOriginsToTopLeft(layout);
  applyHorizontalPerLine(layout, box, options.horizontalAlignment);

  float const h = layout.measuredSize.height;

  float dy = 0.f;
  switch (options.verticalAlignment) {
  case VerticalAlignment::Top:
    dy = 0.f;
    break;
  case VerticalAlignment::FirstBaseline:
    dy = options.firstBaselineOffset - layout.firstBaseline;
    break;
  case VerticalAlignment::Center:
    dy = (box.height - h) * 0.5f;
    break;
  case VerticalAlignment::Bottom:
    dy = box.height - h;
    break;
  }

  for (auto& pr : layout.runs) {
    pr.origin.y += dy;
  }
  for (auto& lr : layout.lines) {
    lr.top += dy;
    lr.bottom += dy;
    lr.baseline += dy;
  }
  recomputeTextLayoutMetrics(layout);
}

} // namespace detail

std::shared_ptr<TextLayout> cloneTextLayout(TextLayout const& src) {
  auto out = std::make_shared<TextLayout>();
  out->runs = src.runs;
  out->lines = src.lines;
  out->measuredSize = src.measuredSize;
  out->firstBaseline = src.firstBaseline;
  out->lastBaseline = src.lastBaseline;
  return out;
}

void trimTextLayoutToMaxLines(TextLayout& layout, int maxLines, bool normalizeAfter) {
  if (maxLines <= 0 || layout.runs.empty()) {
    return;
  }

  std::vector<float> baselines;
  baselines.reserve(layout.runs.size());
  for (auto const& pr : layout.runs) {
    float const y = pr.origin.y;
    bool found = false;
    for (float b : baselines) {
      if (nearLine(b, y)) {
        found = true;
        break;
      }
    }
    if (!found) {
      baselines.push_back(y);
    }
  }
  std::sort(baselines.begin(), baselines.end());
  if (static_cast<int>(baselines.size()) <= maxLines) {
    return;
  }

  std::vector<float> const keep(baselines.begin(), baselines.begin() + static_cast<std::ptrdiff_t>(maxLines));
  auto newEnd = std::remove_if(layout.runs.begin(), layout.runs.end(), [&](TextLayout::PlacedRun const& pr) {
    for (float k : keep) {
      if (nearLine(k, pr.origin.y)) {
        return false;
      }
    }
    return true;
  });
  layout.runs.erase(newEnd, layout.runs.end());

  std::unordered_set<std::uint32_t> keptLineIndices;
  keptLineIndices.reserve(layout.runs.size());
  for (auto const& pr : layout.runs) {
    keptLineIndices.insert(pr.ctLineIndex);
  }
  layout.lines.erase(std::remove_if(layout.lines.begin(), layout.lines.end(),
                                    [&](TextLayout::LineRange const& lr) {
                                      return keptLineIndices.count(lr.ctLineIndex) == 0;
                                    }),
                     layout.lines.end());

  if (normalizeAfter) {
    detail::normalizeOriginsToTopLeft(layout);
  } else {
    recomputeTextLayoutMetrics(layout);
  }
}

std::shared_ptr<TextLayout const> TextSystem::layout(AttributedString const& text, Rect const& box,
                                                     TextLayoutOptions const& options) {
  float const maxWidth = options.wrapping == TextWrapping::NoWrap ? 0.f : box.width;
  std::shared_ptr<TextLayout const> base = layout(text, maxWidth, options);
  if (!base) {
    return nullptr;
  }
  std::shared_ptr<TextLayout> mut = cloneTextLayout(*base);
  detail::applyBoxOptions(*mut, box, options);
  return mut;
}

std::shared_ptr<TextLayout const> TextSystem::layout(std::string_view utf8, Font const& font, Color const& color,
                                                     Rect const& box, TextLayoutOptions const& options) {
  float const maxWidth = options.wrapping == TextWrapping::NoWrap ? 0.f : box.width;
  std::shared_ptr<TextLayout const> base = layout(utf8, font, color, maxWidth, options);
  if (!base) {
    return nullptr;
  }
  std::shared_ptr<TextLayout> mut = cloneTextLayout(*base);
  detail::applyBoxOptions(*mut, box, options);
  return mut;
}

} // namespace flux
