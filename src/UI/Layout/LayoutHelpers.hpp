#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Alignment.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

/// Main-axis span for stacks. Unlike \ref assignedSpan (which prefers the laid-out parent frame when
/// it is positive), when both a positive parent span and a positive finite constraint exist we use the
/// **constraint cap** (`outerSpan`) so nested flex and stacks respect the viewport max width/height.
inline float stackMainAxisSpan(float parentSpan, float outerSpan) {
  if (std::isfinite(outerSpan) && outerSpan > 0.f) {
    return outerSpan;
  }

  return std::max(parentSpan, 0.f);
}

/// Ensures `minWidth` / `minHeight` do not exceed finite `maxWidth` / `maxHeight` (e.g. when a parent
/// root uses min=max=window and a stack assigns a smaller cross-axis or main-axis cap to a child).
inline void clampLayoutMinToMax(LayoutConstraints& c) noexcept {
  if (std::isfinite(c.maxWidth) && c.minWidth > c.maxWidth) {
    c.minWidth = c.maxWidth;
  }
  if (std::isfinite(c.maxHeight) && c.minHeight > c.maxHeight) {
    c.minHeight = c.maxHeight;
  }
}

inline float hAlignOffset(float childW, float innerW, Alignment a) {
  switch (a) {
  case Alignment::Start:
  case Alignment::Stretch:
    return 0.f;
  case Alignment::Center:
    return (innerW - childW) * 0.5f;
  case Alignment::End:
    return innerW - childW;
  }
  return 0.f;
}

inline float vAlignOffset(float childH, float innerH, Alignment a) {
  switch (a) {
  case Alignment::Start:
  case Alignment::Stretch:
    return 0.f;
  case Alignment::Center:
    return (innerH - childH) * 0.5f;
  case Alignment::End:
    return innerH - childH;
  }
  return 0.f;
}

constexpr float kFlexEpsilon = 1e-4f;

/// Opt-in layout diagnostics (same env as the layout tree dump). When set, stderr may include
/// flex-ineffectiveness hints.
inline bool layoutDebugLayoutEnabled() {
  static int cached = -1;
  if (cached < 0) {
    char const* v = std::getenv("FLUX_DEBUG_LAYOUT");
    cached = (v && v[0] != '\0' && std::strcmp(v, "0") != 0) ? 1 : 0;
  }
  return cached != 0;
}

/// Warn when `flexGrow > 0` on a child cannot apply because the stack has no finite main-axis size.
inline void warnFlexGrowIfParentMainAxisUnconstrained(std::vector<Element> const& children,
                                                    bool parentMainAxisFinite) {
  if (parentMainAxisFinite || !layoutDebugLayoutEnabled()) {
    return;
  }
  for (Element const& ch : children) {
    if (ch.flexGrow() > kFlexEpsilon) {
      std::fprintf(stderr,
                   "[flux:layout] flexGrow>0 has no effect: parent stack has no finite main-axis size "
                   "(FLUX_DEBUG_LAYOUT)\n");
      break;
    }
  }
}

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
