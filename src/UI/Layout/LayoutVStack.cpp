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

void Element::Model<VStack>::build(BuildContext& ctx) const {
  ContainerBuildScope scope(ctx);
  float const assignedW = stackMainAxisSpan(scope.parentFrame.width, scope.outer.maxWidth);
  float const assignedH = stackMainAxisSpan(scope.parentFrame.height, scope.outer.maxHeight);
  scope.pushStandardLayer(value.clip, assignedW, assignedH);

  float innerW = std::max(0.f, assignedW - 2.f * value.padding);

  LayoutConstraints childCs = scope.outer;
  childCs.maxHeight = std::numeric_limits<float>::infinity();
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();

  auto sizes = scope.measureChildren(value.children, childCs);
  std::size_t const n = value.children.size();

  float maxChildW = 0.f;
  for (Size s : sizes) {
    maxChildW = std::max(maxChildW, s.width);
  }
  if (innerW <= 0.f) {
    innerW = maxChildW;
  }

  LayoutConstraints innerForBuild = scope.outer;
  innerForBuild.maxWidth = innerW;
  innerForBuild.maxHeight = std::numeric_limits<float>::infinity();

  std::vector<float> allocH(n);
  for (std::size_t i = 0; i < n; ++i) {
    allocH[i] = std::max(sizes[i].height, value.children[i].minMainSize());
  }

  bool const heightConstrained = std::isfinite(assignedH) && assignedH > 0.f;
  if (heightConstrained && n > 0) {
    float const innerH = std::max(0.f, assignedH - 2.f * value.padding);
    float const gaps = n > 1 ? static_cast<float>(n - 1) * value.spacing : 0.f;
    float const targetSum = std::max(0.f, innerH - gaps);
    float sumNat = 0.f;
    for (float h : allocH) {
      sumNat += h;
    }
    float const extra = targetSum - sumNat;
    if (extra > kFlexEpsilon) {
      flexGrowAlongMainAxis(allocH, value.children, extra);
    } else if (extra < -kFlexEpsilon) {
      flexShrinkAlongMainAxis(allocH, value.children, targetSum);
    }
  }

  float y = value.padding;
  for (std::size_t i = 0; i < n; ++i) {
    Size sz = sizes[i];
    sz.height = allocH[i];
    float const rowW = innerW > 0.f ? innerW : sz.width;
    LayoutConstraints childBuild = innerForBuild;
    childBuild.maxHeight = allocH[i];
    childBuild.minHeight = value.children[i].minMainSize();
    childBuild.hStackCrossAlign = std::nullopt;
    childBuild.vStackCrossAlign = value.hAlign;
    scope.buildChild(value.children[i], Rect{value.padding, y, rowW, sz.height}, childBuild);
    y += sz.height + value.spacing;
  }
}

Size Element::Model<VStack>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                     TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  float const assignedW =
      std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float innerW = std::max(0.f, assignedW - 2.f * value.padding);

  LayoutConstraints childCs = constraints;
  childCs.maxHeight = std::numeric_limits<float>::infinity();
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  childCs.hStackCrossAlign = std::nullopt;
  childCs.vStackCrossAlign = value.hAlign;

  float maxW = 0.f;
  float sumH = 2.f * value.padding;
  std::size_t n = value.children.size();
  if (n > 1) {
    sumH += static_cast<float>(n - 1) * value.spacing;
  }
  for (Element const& ch : value.children) {
    Size const s = ch.measure(ctx, childCs, ts);
    maxW = std::max(maxW, s.width);
    sumH += s.height;
  }
  float w = maxW + 2.f * value.padding;
  if (std::isfinite(assignedW) && assignedW > 0.f) {
    w = std::max(w, assignedW);
  }
  return {w, sumH};
}

} // namespace flux
