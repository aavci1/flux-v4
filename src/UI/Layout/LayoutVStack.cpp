#include <Flux/UI/Element.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>
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

void VStack::layout(LayoutContext& ctx) const {
  ContainerLayoutScope scope(ctx);
  float const assignedW = stackMainAxisSpan(scope.parentFrame.width, scope.outer.maxWidth);
  float const assignedH = stackMainAxisSpan(scope.parentFrame.height, scope.outer.maxHeight);
  scope.pushStandardLayer(false, assignedW, assignedH);

  float innerW = std::max(0.f, assignedW);

  LayoutConstraints childCs = scope.outer;
  childCs.maxHeight = std::numeric_limits<float>::infinity();
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();

  LayoutNode const* retainedContainer = ctx.tree().retainedNodeForKey(scope.stableKey());
  bool const canDirectReuseChildren =
      retainedContainer && retainedContainer->kind == LayoutNode::Kind::Container &&
      retainedContainer->children.size() == children.size() &&
      retainedContainer->containerSpec.kind == ContainerLayerSpec::Kind::Standard;
  std::vector<LayoutNodeId> retainedChildRoots;
  if (canDirectReuseChildren) {
    retainedChildRoots.assign(retainedContainer->children.begin(), retainedContainer->children.end());
  }

  LayoutHints measureHints{};
  measureHints.vStackCrossAlign = alignment;
  std::size_t const n = children.size();
  auto sizes = scope.measureChildren(children, childCs, measureHints);
  scope.logContainer("VStack");

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
  clampLayoutMinToMax(innerForBuild);

  std::vector<float> allocH(n);
  for (std::size_t i = 0; i < n; ++i) {
    allocH[i] = std::max(sizes[i].height, children[i].minMainSize());
  }

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

  float usedH = 0.f;
  if (n > 1) {
    usedH += static_cast<float>(n - 1) * spacing;
  }
  for (float h : allocH) {
    usedH += h;
  }

  float y = 0.f;
  if (heightConstrained) {
    y = std::max(0.f, (assignedH - usedH) * 0.5f);
  }
  for (std::size_t i = 0; i < n; ++i) {
    Size sz = sizes[i];
    sz.height = allocH[i];
    float const rowW = innerW > 0.f ? innerW : sz.width;
    Rect const rowFrame{0.f, y, rowW, sz.height};
    LayoutConstraints childBuild = innerForBuild;
    childBuild.maxHeight = allocH[i];
    childBuild.minHeight = children[i].minMainSize();
    clampLayoutMinToMax(childBuild);
    LayoutHints rowHints{};
    rowHints.vStackCrossAlign = alignment;
    bool reusedChild = false;
    if (canDirectReuseChildren) {
      ComponentKey childKey = scope.stableKey();
      childKey.push_back(i);
      LayoutNodeId const retainedChildRoot = retainedChildRoots[i];
      reusedChild = children[i].tryRetainedLayout(ctx, childKey, retainedChildRoot, rowFrame,
                                                  childBuild, rowHints);
    }
    if (!reusedChild) {
      scope.layoutChild(children[i], rowFrame, childBuild, rowHints);
    }
    y += sz.height + spacing;
  }
}

void VStack::renderFromLayout(RenderContext&, LayoutNode const&) const {}

Size VStack::measure(LayoutContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                     TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  float const assignedW =
      std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
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
