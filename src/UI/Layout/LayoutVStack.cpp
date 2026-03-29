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

void Element::Model<VStack>::build(BuildContext& ctx) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine& le = ctx.layoutEngine();
  Rect const parentFrame = le.childFrame();
  LayoutConstraints const outer = ctx.constraints();

  float const assignedW = assignedSpan(parentFrame.width, outer.maxWidth);
  float const assignedH = assignedSpan(parentFrame.height, outer.maxHeight);

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

  float innerW = std::max(0.f, assignedW - 2.f * value.padding);

  LayoutConstraints childCs = outer;
  childCs.maxHeight = std::numeric_limits<float>::infinity();
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();

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

  float maxChildW = 0.f;
  for (Size s : sizes) {
    maxChildW = std::max(maxChildW, s.width);
  }
  if (innerW <= 0.f) {
    innerW = maxChildW;
  }

  LayoutConstraints innerForBuild = outer;
  innerForBuild.maxWidth = innerW;
  innerForBuild.maxHeight = std::numeric_limits<float>::infinity();

  std::vector<float> allocH(n);
  for (std::size_t i = 0; i < n; ++i) {
    allocH[i] = sizes[i].height;
  }

  // Grow and shrink need a finite assigned main-axis size from the parent. If `assignedH` is not
  // set (e.g. nested in an unconstrained-height VStack), we keep natural sizes — same trade-off as
  // measuring with unbounded maxHeight.
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
    Element const& child = value.children[i];
    Size sz = sizes[i];
    sz.height = allocH[i];
    // Stretch each row to the column width so children (e.g. HStack + Spacer) receive the full proposed width.
    // Using only sz.width leaves rows at intrinsic width; a narrow header under a wide wrapped body row would
    // not give flex children (spacers) any extra space.
    float const rowW = std::max(sz.width, innerW);
    float const x = hAlignOffset(rowW, innerW, value.hAlign) + value.padding;
    le.setChildFrame(Rect{x, y, rowW, sz.height});
    LayoutConstraints childBuild = innerForBuild;
    childBuild.maxHeight = allocH[i];
    childBuild.minHeight = child.minMainSize();
    ctx.pushConstraints(childBuild);
    child.build(ctx);
    ctx.popConstraints();
    y += sz.height + value.spacing;
  }

  ctx.popChildIndex();
  ctx.popLayer();
}

Size Element::Model<VStack>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                     TextSystem& ts) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine tmp{};
  float const assignedW =
      std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float innerW = std::max(0.f, assignedW - 2.f * value.padding);

  LayoutConstraints childCs = constraints;
  childCs.maxHeight = std::numeric_limits<float>::infinity();
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();

  float maxW = 0.f;
  float sumH = 2.f * value.padding;
  std::size_t n = value.children.size();
  if (n > 1) {
    sumH += static_cast<float>(n - 1) * value.spacing;
  }
  ctx.pushChildIndex();
  for (Element const& ch : value.children) {
    Size const s = tmp.measure(ctx, ch, childCs, ts);
    maxW = std::max(maxW, s.width);
    sumH += s.height;
  }
  ctx.popChildIndex();
  float w = maxW + 2.f * value.padding;
  if (std::isfinite(assignedW) && assignedW > 0.f) {
    w = std::max(w, assignedW);
  }
  return {w, sumH};
}

} // namespace flux
