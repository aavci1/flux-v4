#include <Flux/UI/Element.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Layout.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/StateStore.hpp>

#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include "UI/Layout/LayoutHelpers.hpp"

#include <cmath>
#include <cstddef>
#include <limits>

namespace flux {
using namespace flux::layout;

void Element::Model<ScaleAroundCenter>::build(BuildContext& ctx) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine& le = ctx.layoutEngine();
  Rect const parentFrame = le.childFrame();
  LayoutConstraints const outer = ctx.constraints();

  float const assignedW = assignedSpan(parentFrame.width, outer.maxWidth);
  float const assignedH = assignedSpan(parentFrame.height, outer.maxHeight);

  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  LayoutConstraints childCs = outer;
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  childCs.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();

  ctx.pushChildIndex();
  Size const sz = le.measure(ctx, value.child, childCs, ctx.textSystem());
  if (StateStore* store = StateStore::current()) {
    store->resetSlotCursors();
  }
  ctx.rewindChildKeyIndex();

  if (innerW <= 0.f) {
    innerW = sz.width;
  }
  if (innerH <= 0.f) {
    innerH = sz.height;
  }

  float const cx = innerW * 0.5f;
  float const cy = innerH * 0.5f;

  LayerNode layer{};
  Mat3 const t = Mat3::translate(parentFrame.x, parentFrame.y) * Mat3::translate(cx, cy) * Mat3::scale(value.scale) *
                 Mat3::translate(-cx, -cy);
  layer.transform = t;
  NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
  ctx.registerCompositeSubtreeRootIfPending(layerId);
  ctx.pushLayer(layerId);

  float const childW = std::max(sz.width, innerW);
  float const childH = std::max(sz.height, innerH);
  float const x = hAlignOffset(childW, innerW, HorizontalAlignment::Center);
  float const y = vAlignOffset(childH, innerH, VerticalAlignment::Center);
  le.setChildFrame(Rect{x, y, childW, childH});

  LayoutConstraints innerForBuild{};
  innerForBuild.maxWidth = innerW;
  innerForBuild.maxHeight = innerH;
  ctx.pushConstraints(innerForBuild);
  value.child.build(ctx);
  ctx.popConstraints();

  ctx.popChildIndex();
  ctx.popLayer();
}

Size Element::Model<ScaleAroundCenter>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
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

  ctx.pushChildIndex();
  Size const s = value.child.measure(ctx, childCs, ts);
  ctx.popChildIndex();
  return s;
}

} // namespace flux
