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
#include <optional>
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
    allocH[i] = std::max(sizes[i].height, value.children[i].minMainSize());
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
    // Full column width for the row slot so nested HStacks flex to innerW without drawing past the
    // slot (narrow frame + stackMainAxisSpan overflow). Cross-axis alignment uses vStackCrossAlign.
    float const rowW = innerW > 0.f ? innerW : sz.width;
    float const x = value.padding;
    le.setChildFrame(Rect{x, y, rowW, sz.height});
    LayoutConstraints childBuild = innerForBuild;
    childBuild.maxHeight = allocH[i];
    childBuild.minHeight = child.minMainSize();
    childBuild.hStackCrossAlign = std::nullopt;
    childBuild.vStackCrossAlign = value.hAlign;
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
    Size const s = ch.measure(ctx, childCs, ts);
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
