#include <Flux/UI/Element.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Layout.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/StateStore.hpp>

#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {
using namespace flux::layout;

void Element::Model<HStack>::build(BuildContext& ctx) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
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

  std::vector<Size> sizes;
  sizes.reserve(value.children.size());
  ctx.pushChildIndex();
  for (Element const& ch : value.children) {
    sizes.push_back(le.measure(ctx, ch, childCs, ctx.textSystem()));
  }
  if (StateStore* store = StateStore::current()) {
    store->resetSlotCursors();
  }
  ctx.rewindChildKeyIndex();

  std::size_t const n = value.children.size();

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
    // See `LayoutConstraints::hStackCrossAlign` — only Rectangle consumes this today.
    childBuild.hStackCrossAlign = value.vAlign;
    ctx.pushConstraints(childBuild);
    value.children[i].build(ctx);
    ctx.popConstraints();
    x += sz.width + value.spacing;
  }

  ctx.popChildIndex();
  ctx.popLayer();
}

Size Element::Model<HStack>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                     TextSystem& ts) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine tmp{};

  LayoutConstraints childCs = constraints;
  childCs.maxWidth = std::numeric_limits<float>::infinity();
  childCs.maxHeight = std::numeric_limits<float>::infinity();

  float sumW = 2.f * value.padding;
  float maxH = 0.f;
  std::size_t n = value.children.size();
  if (n > 1) {
    sumW += static_cast<float>(n - 1) * value.spacing;
  }
  ctx.pushChildIndex();
  for (Element const& ch : value.children) {
    Size const s = tmp.measure(ctx, ch, childCs, ts);
    sumW += s.width;
    maxH = std::max(maxH, s.height);
  }
  ctx.popChildIndex();
  return {sumW, maxH + 2.f * value.padding};
}

} // namespace flux
