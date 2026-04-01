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

void Element::Model<ZStack>::build(BuildContext& ctx) const {
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

  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  LayoutConstraints childCs = outer;
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  childCs.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();

  float maxW = 0.f;
  float maxH = 0.f;
  std::vector<Size> sizes;
  sizes.reserve(value.children.size());
  ctx.pushChildIndex();
  for (Element const& ch : value.children) {
    Size const s = ch.measure(ctx, childCs, ctx.textSystem());
    sizes.push_back(s);
    maxW = std::max(maxW, s.width);
    maxH = std::max(maxH, s.height);
  }
  if (StateStore* store = StateStore::current()) {
    store->resetSlotCursors();
  }
  ctx.rewindChildKeyIndex();

  if (innerW <= 0.f) {
    innerW = maxW;
  }
  if (innerH <= 0.f) {
    innerH = maxH;
  }
  if (value.clip) {
    // Clipped stacks represent a viewport; keep finite assigned size so descendants
    // (e.g. ScrollView) don't see content-sized viewport constraints.
    if (assignedW > 0.f) {
      innerW = assignedW;
    }
    if (assignedH > 0.f) {
      innerH = assignedH;
    }
  } else {
    // Same as `measure`: footprint must match max(intrinsic, proposed) so alignment and
    // `innerForBuild` use the expanded box, not only the parent proposal.
    innerW = std::max(innerW, maxW);
    innerH = std::max(innerH, maxH);
  }

  LayoutConstraints innerForBuild{};
  innerForBuild.maxWidth = innerW;
  innerForBuild.maxHeight = innerH;

  for (std::size_t i = 0; i < value.children.size(); ++i) {
    Size const sz = sizes[i];
    // All stack children share the same layout box (max of measured sizes). Using each child's
    // intrinsic size alone leaves a small VStack behind a full-window Rect with a narrow frame,
    // so flex layouts (HStack + Spacer) never receive the full proposed width.
    float const childW = std::max(sz.width, innerW);
    float const childH = std::max(sz.height, innerH);
    // Align the child's *intrinsic* box in the inner rect (like Grid). Using expanded childW for
    // offsets makes childW == innerW whenever inner wins, so hAlign/vAlign have no effect. Flex
    // children use the expanded frame for alignment so they stay full-size (flexGrow > 0).
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
    // Horizontal strip (no intrinsic width): when the stack expands this child to `innerH` on the
    // vertical axis, align using the expanded frame — otherwise vAlign centers the intrinsic height
    // in `innerH` while childH stays expanded, shifting the layer vs siblings (e.g. Button fill vs
    // label). (Do not mirror for vertical strips: fixed width + stretch height still needs intrinsic
    // width centering when inner is wider.)
    if (childEl.flexGrow() == 0.f) {
      if (sz.width <= 0.f && sz.height > 0.f && childH > sz.height) {
        alignH = childH;
      }
    }
    float const x = hAlignOffset(alignW, innerW, value.hAlign);
    float const y = vAlignOffset(alignH, innerH, value.vAlign);
    // Use alignW/alignH (natural size) as frame dimensions so leaf elements (e.g. Text) that
    // self-align internally don't double-center within an oversized childW × childH frame.
    // Flex children and zero-width strips already have alignW/alignH == childW/childH so
    // their behaviour is unchanged. resolveRectangleBounds for Rectangle with an explicit
    // frame ignores childFrame size and only uses childFrame.x/y, so that path is unaffected.
    le.setChildFrame(Rect{x, y, alignW, alignH});
    ctx.pushConstraints(innerForBuild);
    value.children[i].build(ctx);
    ctx.popConstraints();
  }

  ctx.popChildIndex();
  ctx.popLayer();
}

Size Element::Model<ZStack>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                     TextSystem& ts) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  float const assignedW = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const assignedH = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  LayoutConstraints childCs = constraints;
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  childCs.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();

  float maxW = 0.f;
  float maxH = 0.f;
  ctx.pushChildIndex();
  for (Element const& ch : value.children) {
    Size const s = ch.measure(ctx, childCs, ts);
    maxW = std::max(maxW, s.width);
    maxH = std::max(maxH, s.height);
  }
  ctx.popChildIndex();
  if (innerW <= 0.f) {
    innerW = maxW;
  }
  if (innerH <= 0.f) {
    innerH = maxH;
  }
  if (value.clip) {
    // Clipped stacks should report the assigned viewport size when finite.
    if (assignedW > 0.f) {
      innerW = assignedW;
    }
    if (assignedH > 0.f) {
      innerH = assignedH;
    }
  } else {
    // Match `build`: each child uses max(intrinsic, inner); the stack footprint is at least the
    // maximum of children and the proposed box on each axis.
    innerW = std::max(innerW, maxW);
    innerH = std::max(innerH, maxH);
  }
  return {innerW, innerH};
}

} // namespace flux
