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

void Element::Model<ZStack>::build(BuildContext& ctx) const {
  ContainerBuildScope scope(ctx);
  float const assignedW = assignedSpan(scope.parentFrame.width, scope.outer.maxWidth);
  float const assignedH = assignedSpan(scope.parentFrame.height, scope.outer.maxHeight);
  scope.pushStandardLayer(value.clip, assignedW, assignedH);

  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  LayoutConstraints childCs = scope.outer;
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  childCs.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();

  float maxW = 0.f;
  float maxH = 0.f;
  auto sizes = scope.measureChildren(value.children, childCs);
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
  if (value.clip) {
    if (assignedW > 0.f) {
      innerW = assignedW;
    }
    if (assignedH > 0.f) {
      innerH = assignedH;
    }
  } else {
    innerW = std::max(innerW, maxW);
    innerH = std::max(innerH, maxH);
  }

  LayoutConstraints innerForBuild{};
  innerForBuild.maxWidth = innerW;
  innerForBuild.maxHeight = innerH;

  for (std::size_t i = 0; i < value.children.size(); ++i) {
    Size const sz = sizes[i];
    float const childW = std::max(sz.width, innerW);
    float const childH = std::max(sz.height, innerH);
    float alignW = childW;
    float alignH = childH;
    Element const& childEl = value.children[i];
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
    float const x = hAlignOffset(alignW, innerW, value.hAlign);
    float const y = vAlignOffset(alignH, innerH, value.vAlign);
    scope.buildChild(value.children[i], Rect{x, y, alignW, alignH}, innerForBuild);
  }
}

Size Element::Model<ZStack>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                     LayoutHints const&, TextSystem& ts) const {
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
  for (Element const& ch : value.children) {
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
  if (value.clip) {
    if (assignedW > 0.f) {
      innerW = assignedW;
    }
    if (assignedH > 0.f) {
      innerH = assignedH;
    }
  } else {
    innerW = std::max(innerW, maxW);
    innerH = std::max(innerH, maxH);
  }
  return {innerW, innerH};
}

} // namespace flux
