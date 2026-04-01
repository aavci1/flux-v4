#include <Flux/UI/Element.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {
using namespace flux::layout;

void Element::Model<HStack>::build(BuildContext& ctx) const {
  ContainerBuildScope scope(ctx);
  float const assignedW = stackMainAxisSpan(scope.parentFrame.width, scope.outer.maxWidth);
  float const assignedH = stackMainAxisSpan(scope.parentFrame.height, scope.outer.maxHeight);
  scope.pushStandardLayer(value.clip, assignedW, assignedH);

  LayoutConstraints childCs = scope.outer;
  childCs.maxWidth = std::numeric_limits<float>::infinity();
  childCs.maxHeight = std::numeric_limits<float>::infinity();

  std::size_t const n = value.children.size();
  if (n == 1 && std::isfinite(scope.outer.maxWidth) && scope.outer.maxWidth > 0.f) {
    childCs.maxWidth = std::max(0.f, scope.outer.maxWidth - 2.f * value.padding);
  }

  auto sizes = scope.measureChildren(value.children, childCs);

  float maxH = 0.f;
  for (std::size_t i = 0; i < n; ++i) {
    maxH = std::max(maxH, sizes[i].height);
  }
  float const rowInnerH = maxH;

  LayoutConstraints innerForBuild = scope.outer;
  innerForBuild.maxWidth = std::numeric_limits<float>::infinity();
  innerForBuild.maxHeight = rowInnerH;

  std::vector<float> allocW(n);
  for (std::size_t i = 0; i < n; ++i) {
    allocW[i] = std::max(sizes[i].width, value.children[i].minMainSize());
  }

  bool const widthConstrained = std::isfinite(assignedW) && assignedW > 0.f;
  if (widthConstrained && n > 0) {
    float const innerW = std::max(0.f, assignedW - 2.f * value.padding);
    float const gaps = n > 1 ? static_cast<float>(n - 1) * value.spacing : 0.f;
    float const targetSum = std::max(0.f, innerW - gaps);
    float sumNat = 0.f;
    for (float w : allocW) {
      sumNat += w;
    }
    float const extra = targetSum - sumNat;
    if (extra > kFlexEpsilon) {
      flexGrowAlongMainAxis(allocW, value.children, extra);
    } else if (extra < -kFlexEpsilon) {
      flexShrinkAlongMainAxis(allocW, value.children, targetSum);
    }
  } else if (n > 0) {
    warnFlexGrowIfParentMainAxisUnconstrained(value.children, widthConstrained);
  }

  float x = value.padding;
  for (std::size_t i = 0; i < n; ++i) {
    LayoutConstraints childBuild = innerForBuild;
    childBuild.maxWidth = allocW[i];
    childBuild.minWidth = value.children[i].minMainSize();
    childBuild.hStackCrossAlign = value.vAlign;
    childBuild.vStackCrossAlign = std::nullopt;
    scope.buildChild(value.children[i], Rect{x, value.padding, allocW[i], rowInnerH}, childBuild);
    x += allocW[i] + value.spacing;
  }
}

Size Element::Model<HStack>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                     TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  LayoutConstraints childCs = constraints;
  childCs.maxWidth = std::numeric_limits<float>::infinity();
  childCs.maxHeight = std::numeric_limits<float>::infinity();

  std::size_t n = value.children.size();
  if (n == 1 && std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
    childCs.maxWidth = std::max(0.f, constraints.maxWidth - 2.f * value.padding);
  }

  float sumW = 2.f * value.padding;
  float maxH = 0.f;
  if (n > 1) {
    sumW += static_cast<float>(n - 1) * value.spacing;
  }
  for (Element const& ch : value.children) {
    Size const s = ch.measure(ctx, childCs, ts);
    sumW += s.width;
    maxH = std::max(maxH, s.height);
  }
  return {sumW, maxH + 2.f * value.padding};
}

} // namespace flux
