#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <cmath>

namespace flux::detail {

inline Rect resolveLeafBounds(Rect const& frame, Rect const& childFrame,
                              LayoutConstraints const& constraints) {
  Rect bounds = frame;
  if (childFrame.width > 0.f || childFrame.height > 0.f) {
    bounds = childFrame;
  }
  if (bounds.width <= 0.f || bounds.height <= 0.f) {
    float const w = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
    float const h = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
    if (w > 0.f && h > 0.f) {
      bounds = Rect{0, 0, w, h};
    }
  }
  return bounds;
}

} // namespace flux::detail
