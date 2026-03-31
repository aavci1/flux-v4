#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux::layout {

/// When the parent assigned a frame (width/height > 0), use it; otherwise use the finite constraint span.
inline float assignedSpan(float parentSpan, float outerSpan) {
  if (parentSpan > 0.f) {
    return parentSpan;
  }
  if (std::isfinite(outerSpan) && outerSpan > 0.f) {
    return outerSpan;
  }
  return 0.f;
}

/// Main-axis span for stack children when the parent frame may be narrower than `outer` (cross-axis
/// alignment) but flex and nested stacks must still use the constraint cap (e.g. column width).
inline float stackMainAxisSpan(float parentSpan, float outerSpan) {
  if (parentSpan > 0.f && std::isfinite(outerSpan) && outerSpan > 0.f) {
    return std::min(outerSpan, std::max(parentSpan, outerSpan));
  }
  return assignedSpan(parentSpan, outerSpan);
}

inline float hAlignOffset(float childW, float innerW, HorizontalAlignment a) {
  switch (a) {
  case HorizontalAlignment::Leading:
    return 0.f;
  case HorizontalAlignment::Center:
    return (innerW - childW) * 0.5f;
  case HorizontalAlignment::Trailing:
    return innerW - childW;
  }
  return 0.f;
}

inline float vAlignOffset(float childH, float innerH, VerticalAlignment a) {
  switch (a) {
  case VerticalAlignment::Top:
  case VerticalAlignment::FirstBaseline:
    return 0.f;
  case VerticalAlignment::Center:
    return (innerH - childH) * 0.5f;
  case VerticalAlignment::Bottom:
    return innerH - childH;
  }
  return 0.f;
}

/// Per-row height when the grid has a finite inner height (`innerH > 0`); otherwise 0 (unconstrained).
inline float gridCellHeight(float innerH, std::size_t rowCount, float vSpacing) {
  if (innerH <= 0.f || rowCount == 0) {
    return 0.f;
  }
  float const gaps = rowCount > 1 ? static_cast<float>(rowCount - 1) * vSpacing : 0.f;
  return std::max(0.f, (innerH - gaps) / static_cast<float>(rowCount));
}

constexpr float kFlexEpsilon = 1e-4f;

inline void flexGrowAlongMainAxis(std::vector<float>& alloc, std::vector<Element> const& children,
                                  float extra) {
  if (extra <= kFlexEpsilon) {
    return;
  }
  float totalGrow = 0.f;
  for (Element const& ch : children) {
    totalGrow += ch.flexGrow();
  }
  if (totalGrow <= kFlexEpsilon) {
    return;
  }
  for (std::size_t i = 0; i < children.size(); ++i) {
    float const g = children[i].flexGrow();
    if (g > 0.f) {
      alloc[i] += extra * (g / totalGrow);
    }
  }
}

/// Reduces main-axis allocations until `sum(alloc) <= targetSum` (or no shrinkable space left).
/// Shrink weights follow CSS flexbox: proportional to `flexShrink * naturalSize`, using a snapshot
/// of each child's natural main size at shrink start (not the current allocation after min clamps).
inline void flexShrinkAlongMainAxis(std::vector<float>& alloc, std::vector<Element> const& children,
                                    float targetSum) {
  std::vector<float> const natural = alloc;
  for (;;) {
    float sumA = 0.f;
    for (float a : alloc) {
      sumA += a;
    }
    float const need = sumA - targetSum;
    if (need <= kFlexEpsilon) {
      break;
    }
    float sumBasis = 0.f;
    for (std::size_t i = 0; i < children.size(); ++i) {
      float const fs = children[i].flexShrink();
      float const mn = children[i].minMainSize();
      if (fs > 0.f && alloc[i] > mn + kFlexEpsilon) {
        sumBasis += fs * natural[i];
      }
    }
    if (sumBasis <= 1e-6f) {
      break;
    }
    float removedPass = 0.f;
    for (std::size_t i = 0; i < children.size(); ++i) {
      float const fs = children[i].flexShrink();
      float const mn = children[i].minMainSize();
      if (fs <= 0.f || alloc[i] <= mn + kFlexEpsilon) {
        continue;
      }
      float const r = need * (fs * natural[i] / sumBasis);
      float const na = alloc[i] - r;
      if (na < mn) {
        removedPass += alloc[i] - mn;
        alloc[i] = mn;
      } else {
        removedPass += r;
        alloc[i] = na;
      }
    }
    if (removedPass < kFlexEpsilon) {
      break;
    }
  }
}

} // namespace flux::layout
