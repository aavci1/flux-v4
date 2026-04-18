#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {
using namespace flux::layout;

Size VStack::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                     TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  float const assignedW = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float innerW = std::max(0.f, assignedW);

  LayoutConstraints childCs = constraints;
  childCs.maxHeight = std::numeric_limits<float>::infinity();
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  LayoutHints childHints{};
  childHints.vStackCrossAlign = alignment;

  std::vector<Size> sizes;
  sizes.reserve(children.size());
  float maxW = 0.f;
  std::size_t n = children.size();
  for (Element const& ch : children) {
    Size const s = ch.measure(ctx, childCs, childHints, ts);
    sizes.push_back(s);
    maxW = std::max(maxW, s.width);
  }

  std::vector<float> allocH(n);
  for (std::size_t i = 0; i < n; ++i) {
    allocH[i] = std::max(sizes[i].height, children[i].minMainSize());
  }

  float const assignedH = stackMainAxisSpan(0.f, constraints.maxHeight);
  bool const heightConstrained = std::isfinite(assignedH) && assignedH > 0.f;
  if (heightConstrained && n > 0) {
    float const innerH = std::max(0.f, assignedH);
    float const gaps = n > 1 ? static_cast<float>(n - 1) * spacing : 0.f;
    float const targetSum = std::max(0.f, innerH - gaps);
    float sumNat = 0.f;
    for (float h : allocH) {
      sumNat += h;
    }
    float const extra = targetSum - sumNat;
    if (extra > kFlexEpsilon) {
      flexGrowAlongMainAxis(allocH, children, extra);
    } else if (extra < -kFlexEpsilon) {
      flexShrinkAlongMainAxis(allocH, children, targetSum);
    }
  } else if (n > 0) {
    warnFlexGrowIfParentMainAxisUnconstrained(children, heightConstrained);
  }

  float sumH = 0.f;
  if (n > 1) {
    sumH += static_cast<float>(n - 1) * spacing;
  }
  for (float h : allocH) {
    sumH += h;
  }
  float w = maxW;
  if (std::isfinite(assignedW) && assignedW > 0.f) {
    w = std::max(w, assignedW);
  }
  return {w, sumH};
}

} // namespace flux
