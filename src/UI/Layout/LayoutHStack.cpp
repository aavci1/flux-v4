#include <Flux/UI/Element.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/RenderContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/Views/HStack.hpp>

#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {
using namespace flux::layout;

void HStack::layout(LayoutContext& ctx) const {
  ContainerLayoutScope scope(ctx);
  float const assignedW = stackMainAxisSpan(scope.parentFrame.width, scope.outer.maxWidth);
  float const assignedH = stackMainAxisSpan(scope.parentFrame.height, scope.outer.maxHeight);
  scope.pushStandardLayer(false, assignedW, assignedH);

  LayoutConstraints childCs = scope.outer;
  childCs.maxWidth = std::numeric_limits<float>::infinity();
  childCs.maxHeight = std::numeric_limits<float>::infinity();

  std::size_t const n = children.size();
  if (n == 1 && std::isfinite(scope.outer.maxWidth) && scope.outer.maxWidth > 0.f) {
    childCs.maxWidth = std::max(0.f, scope.outer.maxWidth);
  }

  auto sizes = scope.measureChildren(children, childCs);
  scope.logContainer("HStack");

  std::vector<float> allocW(n);
  for (std::size_t i = 0; i < n; ++i) {
    allocW[i] = std::max(sizes[i].width, children[i].minMainSize());
  }

  bool const widthConstrained = std::isfinite(assignedW) && assignedW > 0.f;
  if (widthConstrained && n > 0) {
    float const innerW = std::max(0.f, assignedW);
    float const gaps = n > 1 ? static_cast<float>(n - 1) * spacing : 0.f;
    float const targetSum = std::max(0.f, innerW - gaps);
    float sumNat = 0.f;
    for (float w : allocW) {
      sumNat += w;
    }
    float const extra = targetSum - sumNat;
    if (extra > kFlexEpsilon) {
      flexGrowAlongMainAxis(allocW, children, extra);
    } else if (extra < -kFlexEpsilon) {
      flexShrinkAlongMainAxis(allocW, children, targetSum);
    }
  } else if (n > 0) {
    warnFlexGrowIfParentMainAxisUnconstrained(children, widthConstrained);
  }

  float rowInnerH = 0.f;
  for (std::size_t i = 0; i < n; ++i) {
    LayoutConstraints cs2 = scope.outer;
    cs2.maxWidth = allocW[i];
    cs2.maxHeight = std::numeric_limits<float>::infinity();
    LayoutHints rh{};
    rh.hStackCrossAlign = alignment;
    Size const sz2 = children[i].measure(ctx, cs2, rh, ctx.textSystem());
    rowInnerH = std::max(rowInnerH, sz2.height);
  }
  if (alignment == Alignment::Stretch && std::isfinite(assignedH) && assignedH > 0.f) {
    rowInnerH = std::max(rowInnerH, assignedH);
  }
  if (StateStore* store = StateStore::current()) {
    store->resetSlotCursors();
  }
  ctx.rewindChildKeyIndex();

  LayoutConstraints innerForBuild = scope.outer;
  innerForBuild.maxWidth = std::numeric_limits<float>::infinity();
  innerForBuild.maxHeight = rowInnerH;
  clampLayoutMinToMax(innerForBuild);

  float x = 0.f;
  for (std::size_t i = 0; i < n; ++i) {
    LayoutConstraints childBuild = innerForBuild;
    childBuild.maxWidth = allocW[i];
    childBuild.minWidth = children[i].minMainSize();
    clampLayoutMinToMax(childBuild);
    LayoutHints rowHints{};
    rowHints.hStackCrossAlign = alignment;
    scope.layoutChild(children[i], Rect{x, 0.f, allocW[i], rowInnerH}, childBuild, rowHints);
    x += allocW[i] + spacing;
  }
}

void HStack::renderFromLayout(RenderContext&, LayoutNode const&) const {}

Size HStack::measure(LayoutContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                     TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  LayoutConstraints childCs = constraints;
  childCs.maxWidth = std::numeric_limits<float>::infinity();
  childCs.maxHeight = std::numeric_limits<float>::infinity();

  std::size_t const n = children.size();
  if (n == 1 && std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
    childCs.maxWidth = std::max(0.f, constraints.maxWidth);
  }

  if (n == 0) {
    return {0.f, 0.f};
  }

  std::vector<Size> sizes;
  sizes.reserve(n);
  for (Element const& ch : children) {
    Size const s = ch.measure(ctx, childCs, LayoutHints{}, ts);
    sizes.push_back(s);
  }

  float const assignedW = stackMainAxisSpan(0.f, constraints.maxWidth);

  std::vector<float> allocW(n);
  for (std::size_t i = 0; i < n; ++i) {
    allocW[i] = std::max(sizes[i].width, children[i].minMainSize());
  }

  bool const widthConstrained = std::isfinite(assignedW) && assignedW > 0.f;
  if (widthConstrained && n > 0) {
    float const innerW = std::max(0.f, assignedW);
    float const gaps = n > 1 ? static_cast<float>(n - 1) * spacing : 0.f;
    float const targetSum = std::max(0.f, innerW - gaps);
    float sumNat = 0.f;
    for (float w : allocW) {
      sumNat += w;
    }
    float const extra = targetSum - sumNat;
    if (extra > kFlexEpsilon) {
      flexGrowAlongMainAxis(allocW, children, extra);
    } else if (extra < -kFlexEpsilon) {
      flexShrinkAlongMainAxis(allocW, children, targetSum);
    }
  } else if (n > 0) {
    warnFlexGrowIfParentMainAxisUnconstrained(children, widthConstrained);
  }

  if (StateStore* store = StateStore::current()) {
    store->resetSlotCursors();
  }
  ctx.rewindChildKeyIndex();

  float maxH = 0.f;
  for (std::size_t i = 0; i < n; ++i) {
    LayoutConstraints cs2 = constraints;
    cs2.maxWidth = allocW[i];
    cs2.maxHeight = std::numeric_limits<float>::infinity();
    LayoutHints rh{};
    rh.hStackCrossAlign = alignment;
    Size const s2 = children[i].measure(ctx, cs2, rh, ts);
    maxH = std::max(maxH, s2.height);
  }
  if (StateStore* store = StateStore::current()) {
    store->resetSlotCursors();
  }
  ctx.rewindChildKeyIndex();

  float const assignedHCross = stackMainAxisSpan(0.f, constraints.maxHeight);
  float outH = maxH;
  if (alignment == Alignment::Stretch && std::isfinite(assignedHCross) && assignedHCross > 0.f) {
    outH = std::max(outH, assignedHCross);
  }
  float outW = 0.f;
  if (n > 1) {
    outW += static_cast<float>(n - 1) * spacing;
  }
  for (float w : allocW) {
    outW += w;
  }
  return {outW, outH};
}

} // namespace flux
