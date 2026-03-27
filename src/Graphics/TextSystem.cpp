#include <Flux/Graphics/TextSystem.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace flux {

namespace {

constexpr float kLineEps = 0.5f;

void recomputeMetrics(TextLayout& L) {
  if (L.runs.empty()) {
    L.measuredSize = {};
    L.firstBaseline = 0.f;
    L.lastBaseline = 0.f;
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
  recomputeMetrics(L);
}

bool nearLine(float a, float b) { return std::abs(a - b) < kLineEps; }

void trimToMaxLines(TextLayout& L, int maxLines) {
  if (maxLines <= 0 || L.runs.empty()) {
    return;
  }

  std::vector<float> baselines;
  baselines.reserve(L.runs.size());
  for (auto const& pr : L.runs) {
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
  auto newEnd = std::remove_if(L.runs.begin(), L.runs.end(), [&](TextLayout::PlacedRun const& pr) {
    for (float k : keep) {
      if (nearLine(k, pr.origin.y)) {
        return false;
      }
    }
    return true;
  });
  L.runs.erase(newEnd, L.runs.end());
  normalizeOriginsToTopLeft(L);
}

void applyBoxOptions(std::shared_ptr<TextLayout> const& layout, Rect const& box,
                     TextLayoutOptions const& options) {
  if (!layout) {
    return;
  }

  normalizeOriginsToTopLeft(*layout);

  if (options.maxLines > 0) {
    trimToMaxLines(*layout, options.maxLines);
  }

  float const w = layout->measuredSize.width;
  float const h = layout->measuredSize.height;

  float dx = 0.f;
  switch (options.horizontalAlignment) {
  case HorizontalAlignment::Leading:
    dx = 0.f;
    break;
  case HorizontalAlignment::Center:
    dx = (box.width - w) * 0.5f;
    break;
  case HorizontalAlignment::Trailing:
    dx = box.width - w;
    break;
  }

  float dy = 0.f;
  switch (options.verticalAlignment) {
  case VerticalAlignment::Top:
  case VerticalAlignment::FirstBaseline:
    dy = 0.f;
    break;
  case VerticalAlignment::Center:
    dy = (box.height - h) * 0.5f;
    break;
  case VerticalAlignment::Bottom:
    dy = box.height - h;
    break;
  }

  for (auto& pr : layout->runs) {
    pr.origin.x += dx;
    pr.origin.y += dy;
  }
}

} // namespace

std::shared_ptr<TextLayout> TextSystem::layout(AttributedString const& text, Rect const& box,
                                               TextLayoutOptions const& options) {
  float const maxWidth = options.wrapping == TextWrapping::NoWrap ? 0.f : box.width;
  std::shared_ptr<TextLayout> result = layout(text, maxWidth, options);
  applyBoxOptions(result, box, options);
  return result;
}

std::shared_ptr<TextLayout> TextSystem::layout(std::string_view utf8, TextAttribute const& attr, Rect const& box,
                                               TextLayoutOptions const& options) {
  float const maxWidth = options.wrapping == TextWrapping::NoWrap ? 0.f : box.width;
  std::shared_ptr<TextLayout> result = layout(utf8, attr, maxWidth, options);
  applyBoxOptions(result, box, options);
  return result;
}

} // namespace flux
