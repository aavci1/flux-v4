#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include "UI/Layout/ContainerScope.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace flux {

Size ZStack::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                     TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  float const assignedW = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const assignedH = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  LayoutConstraints childCs = constraints;
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  childCs.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();

  float maxW = 0.f;
  float maxH = 0.f;
  for (Element const& ch : children) {
    Size const s = ch.measure(ctx, childCs, LayoutHints{}, ts);
    maxW = std::max(maxW, s.width);
    maxH = std::max(maxH, s.height);
  }
  if (innerW <= 0.f) {
    innerW = maxW;
  }
  if (innerH <= 0.f) {
    innerH = maxH;
  }
  innerW = std::max(innerW, maxW);
  innerH = std::max(innerH, maxH);
  if (assignedW > 0.f) {
    innerW = std::min(innerW, assignedW);
  }
  if (assignedH > 0.f) {
    innerH = std::min(innerH, assignedH);
  }
  return {innerW, innerH};
}

} // namespace flux
