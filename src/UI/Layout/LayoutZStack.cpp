#include <Flux/UI/Element.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/RenderContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {
using namespace flux::layout;

void ZStack::layout(LayoutContext& ctx) const {
  ContainerLayoutScope scope(ctx);
  float const assignedW = assignedSpan(scope.parentFrame.width, scope.outer.maxWidth);
  float const assignedH = assignedSpan(scope.parentFrame.height, scope.outer.maxHeight);
  scope.pushStandardLayer(false, assignedW, assignedH);

  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  LayoutConstraints childCs = scope.outer;
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  childCs.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();

  float maxW = 0.f;
  float maxH = 0.f;
  auto sizes = scope.measureChildren(children, childCs);
  scope.logContainer("ZStack");
  for (Size const& s : sizes) {
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
  float const slotW = assignedSpan(scope.parentFrame.width, scope.outer.maxWidth);
  float const slotH = assignedSpan(scope.parentFrame.height, scope.outer.maxHeight);
  if (slotW > 0.f) {
    innerW = std::min(innerW, slotW);
  }
  if (slotH > 0.f) {
    innerH = std::min(innerH, slotH);
  }

  LayoutConstraints innerForBuild{};
  innerForBuild.maxWidth = innerW;
  innerForBuild.maxHeight = innerH;

  for (std::size_t i = 0; i < children.size(); ++i) {
    Size const sz = sizes[i];
    float const childW = std::max(sz.width, innerW);
    float const childH = std::max(sz.height, innerH);
    float alignW = childW;
    float alignH = childH;
    Element const& childEl = children[i];
    if (childEl.flexGrow() == 0.f) {
      if (sz.width > 0.f) {
        alignW = sz.width;
      }
      if (sz.height > 0.f) {
        alignH = sz.height;
      }
    }
    if (childEl.flexGrow() == 0.f) {
      if (sz.width <= 0.f && sz.height > 0.f && childH > sz.height) {
        alignH = childH;
      }
    }
    float const x = hAlignOffset(alignW, innerW, hAlign);
    float const y = vAlignOffset(alignH, innerH, vAlign);
    // Child layout rect must not exceed the stack's inner bounds. Otherwise a tall scroll child
    // (alignH > innerH) still received parentFrame.height == content height and ScrollView saw no range.
    float outW = alignW;
    float outH = alignH;
    if (innerW > 0.f) {
      outW = std::min(alignW, innerW);
    }
    if (innerH > 0.f) {
      outH = std::min(alignH, innerH);
    }
    scope.layoutChild(children[i], Rect{x, y, outW, outH}, innerForBuild);
  }
}

void ZStack::renderFromLayout(RenderContext&, LayoutNode const&) const {}

Size ZStack::measure(LayoutContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
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
