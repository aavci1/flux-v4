#include <Flux/UI/Element.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Layout.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/StateStore.hpp>

#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/UI/TestAnnotate.hpp>

#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {
using namespace flux::layout;

void Element::Model<HStack>::build(BuildContext& ctx) const {
  ComponentKey const __testKey = ctx.leafComponentKey();
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  detail::annotateCompositeEnter(ctx, value, __testKey);
  LayoutEngine& le = ctx.layoutEngine();
  Rect const parentFrame = le.childFrame();
  LayoutConstraints const outer = ctx.constraints();

  float const assignedW = stackMainAxisSpan(parentFrame.width, outer.maxWidth);
  float const assignedH = stackMainAxisSpan(parentFrame.height, outer.maxHeight);

  LayerNode layer{};
  if (parentFrame.width > 0.f || parentFrame.height > 0.f) {
    layer.transform = Mat3::translate(parentFrame.x, parentFrame.y);
  }
  if (value.clip && assignedW > 0.f && assignedH > 0.f) {
    layer.clip = Rect{0.f, 0.f, assignedW, assignedH};
  }
  NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
  ctx.registerCompositeSubtreeRootIfPending(layerId);
  ctx.pushLayer(layerId);

  LayoutConstraints childCs = outer;
  childCs.maxWidth = std::numeric_limits<float>::infinity();
  childCs.maxHeight = std::numeric_limits<float>::infinity();

  std::size_t const n = value.children.size();
  // Match `HStack::measure`: single-child rows must use the parent's finite width so wrapping
  // Text measures multi-line height (`sizes` drives `rowInnerH` and flex basis).
  if (n == 1 && std::isfinite(outer.maxWidth) && outer.maxWidth > 0.f) {
    childCs.maxWidth = std::max(0.f, outer.maxWidth - 2.f * value.padding);
  }

  std::vector<Size> sizes;
  sizes.reserve(value.children.size());
  ctx.pushChildIndex();
  for (Element const& ch : value.children) {
    sizes.push_back(ch.measure(ctx, childCs, ctx.textSystem()));
  }
  if (StateStore* store = StateStore::current()) {
    store->resetSlotCursors();
  }
  ctx.rewindChildKeyIndex();

  float maxH = 0.f;
  for (std::size_t i = 0; i < n; ++i) {
    maxH = std::max(maxH, sizes[i].height);
  }
  float const rowInnerH = maxH;

  LayoutConstraints innerForBuild = outer;
  innerForBuild.maxWidth = std::numeric_limits<float>::infinity();
  innerForBuild.maxHeight = rowInnerH;

  std::vector<float> allocW(n);
  for (std::size_t i = 0; i < n; ++i) {
    // Flex basis must respect min main size; otherwise flex-grow only touches fg>0 children and
    // minMain (e.g. withFlex third arg) is ignored when intrinsic measure is smaller.
    allocW[i] = std::max(sizes[i].width, value.children[i].minMainSize());
  }

  // Grow/shrink along the row only when the stack has a finite assigned width from its parent.
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
  }

  float x = value.padding;
  for (std::size_t i = 0; i < n; ++i) {
    Size sz = sizes[i];
    sz.width = allocW[i];
    // Each column gets the full row height so nested VStacks (e.g. Text + Spacer) receive a
    // definite main-axis size; vertical alignment of shorter intrinsic children is handled
    // inside leaves (e.g. Rectangle with explicit frame + hStackCrossAlign).
    le.setChildFrame(Rect{x, value.padding, allocW[i], rowInnerH});
    LayoutConstraints childBuild = innerForBuild;
    childBuild.maxWidth = allocW[i];
    childBuild.minWidth = value.children[i].minMainSize();
    // See `LayoutConstraints::hStackCrossAlign` — `Rectangle` uses `resolveRectangleBounds`; other
    // leaves typically fill row height.
    childBuild.hStackCrossAlign = value.vAlign;
    childBuild.vStackCrossAlign = std::nullopt;
    ctx.pushConstraints(childBuild);
    value.children[i].build(ctx);
    ctx.popConstraints();
    x += sz.width + value.spacing;
  }

  detail::annotateCompositeExit(ctx);
  ctx.popChildIndex();
  ctx.popLayer();
}

Size Element::Model<HStack>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                     TextSystem& ts) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutConstraints childCs = constraints;
  childCs.maxWidth = std::numeric_limits<float>::infinity();
  childCs.maxHeight = std::numeric_limits<float>::infinity();

  std::size_t n = value.children.size();
  // Single-child rows often wrap Text with flexGrow; measuring with infinite width yields one
  // long line and inflates intrinsic width (VStack/ScrollView content wider than the viewport).
  // When maxWidth is infinite (unconstrained parent), we leave maxWidth infinite — same as multi-child.
  if (n == 1 && std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
    childCs.maxWidth = std::max(0.f, constraints.maxWidth - 2.f * value.padding);
  }

  float sumW = 2.f * value.padding;
  float maxH = 0.f;
  if (n > 1) {
    sumW += static_cast<float>(n - 1) * value.spacing;
  }
  ctx.pushChildIndex();
  for (Element const& ch : value.children) {
    Size const s = ch.measure(ctx, childCs, ts);
    sumW += s.width;
    maxH = std::max(maxH, s.height);
  }
  ctx.popChildIndex();
  return {sumW, maxH + 2.f * value.padding};
}

} // namespace flux
