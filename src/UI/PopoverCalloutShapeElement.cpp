#include <Flux/UI/Element.hpp>

#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Views/PathShape.hpp>
#include <Flux/UI/Views/PopoverCalloutPath.hpp>

#include <Flux/Core/Cursor.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/UI/EventMap.hpp>

#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace flux {

using namespace flux::layout;

namespace {

LayoutConstraints innerConstraintsForPopoverContent(PopoverCalloutShape const& value,
                                                    LayoutConstraints cc) {
  if (value.maxSize) {
    if (std::isfinite(value.maxSize->width) && value.maxSize->width > 0.f) {
      cc.maxWidth = std::min(cc.maxWidth, value.maxSize->width);
    }
    if (std::isfinite(value.maxSize->height) && value.maxSize->height > 0.f) {
      cc.maxHeight = std::min(cc.maxHeight, value.maxSize->height);
    }
  }

  float const pad = value.padding;
  float const ah = PopoverCalloutShape::kArrowH;

  float availW = cc.maxWidth;
  float availH = cc.maxHeight;
  if (std::isfinite(availW)) {
    availW -= 2.f * pad;
  }
  if (std::isfinite(availH)) {
    availH -= 2.f * pad;
  }

  if (value.arrow) {
    switch (value.placement) {
    case PopoverPlacement::Below:
    case PopoverPlacement::Above:
      if (std::isfinite(availH)) {
        availH -= ah;
      }
      break;
    case PopoverPlacement::End:
    case PopoverPlacement::Start:
      if (std::isfinite(availW)) {
        availW -= ah;
      }
      break;
    }
  }

  cc.maxWidth = std::max(0.f, availW);
  cc.maxHeight = std::max(0.f, availH);
  return cc;
}

} // namespace

Size Element::Model<PopoverCalloutShape>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                                    TextSystem& ts) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine tmp{};
  LayoutConstraints cc = innerConstraintsForPopoverContent(value, constraints);

  ctx.pushChildIndex();
  Size const inner = tmp.measure(ctx, value.content, cc, ts);
  ctx.popChildIndex();

  float const pad = value.padding;
  float const ah = PopoverCalloutShape::kArrowH;
  float const awTri = PopoverCalloutShape::kArrowW;

  float const cardW = inner.width + 2.f * pad;
  float const cardH = inner.height + 2.f * pad;

  if (!value.arrow) {
    return {cardW, cardH};
  }
  switch (value.placement) {
  case PopoverPlacement::Below:
  case PopoverPlacement::Above:
    return {cardW, cardH + ah};
  case PopoverPlacement::End:
  case PopoverPlacement::Start:
    return {cardW + ah, std::max(cardH, awTri)};
  }
  return {cardW, cardH};
}

void Element::Model<PopoverCalloutShape>::build(BuildContext& ctx) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine& le = ctx.layoutEngine();
  Rect const parentFrame = le.childFrame();
  LayoutConstraints outer = ctx.constraints();
  // Match measure(): intersect overlay constraints with the popover's intrinsic maxSize. Build used
  // to read only ctx.constraints(); if overlay maxSize was wrong, the card collapsed to one row.
  if (value.maxSize) {
    if (std::isfinite(value.maxSize->width) && value.maxSize->width > 0.f) {
      outer.maxWidth = std::min(outer.maxWidth, value.maxSize->width);
    }
    if (std::isfinite(value.maxSize->height) && value.maxSize->height > 0.f) {
      outer.maxHeight = std::min(outer.maxHeight, value.maxSize->height);
    }
  }

  LayoutConstraints const ccInner = innerConstraintsForPopoverContent(value, outer);
  LayoutEngine tmpMeasure{};
  ctx.pushChildIndex();
  Size const innerMeasured = tmpMeasure.measure(ctx, value.content, ccInner, ctx.textSystem());
  ctx.popChildIndex();

  float const assignedW = assignedSpan(parentFrame.width, outer.maxWidth);
  float const assignedH = assignedSpan(parentFrame.height, outer.maxHeight);

  float const pad = value.padding;
  float const ah = PopoverCalloutShape::kArrowH;

  float tw = std::max(0.f, assignedW);
  float th = std::max(0.f, assignedH);

  // Shrink-wrap height: overlay max height is often the full cap (e.g. 240px) while list content is
  // shorter. Do not clamp width — Popover max width matches the trigger; narrowing would clip it.
  float const cardHFromContent = innerMeasured.height + 2.f * pad;
  if (!value.arrow) {
    if (cardHFromContent > 1e-3f && cardHFromContent < th) {
      th = cardHFromContent;
    }
  } else {
    switch (value.placement) {
    case PopoverPlacement::Below:
    case PopoverPlacement::Above: {
      float const layerH = cardHFromContent + ah;
      if (layerH > 1e-3f && layerH < th) {
        th = layerH;
      }
      break;
    }
    case PopoverPlacement::End:
    case PopoverPlacement::Start:
      if (cardHFromContent > 1e-3f && cardHFromContent < th) {
        th = cardHFromContent;
      }
      break;
    }
  }

  LayerNode layer{};
  if (parentFrame.width > 0.f || parentFrame.height > 0.f) {
    layer.transform = Mat3::translate(parentFrame.x, parentFrame.y);
  }
  NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
  ctx.registerCompositeSubtreeRootIfPending(layerId);
  ctx.pushLayer(layerId);

  // Card size follows clamped tw/th so path + fill match content; still respects overlay max via outer.
  float cardW = tw;
  float cardH = th;
  Rect cardRect{};
  Rect contentFrame{};

  if (!value.arrow) {
    cardRect = {0.f, 0.f, tw, th};
    contentFrame = {pad, pad, std::max(0.f, tw - 2.f * pad), std::max(0.f, th - 2.f * pad)};
  } else {
    switch (value.placement) {
    case PopoverPlacement::Below:
      cardH = std::max(0.f, th - ah);
      cardRect = {0.f, ah, tw, cardH};
      contentFrame = {pad, ah + pad, std::max(0.f, tw - 2.f * pad), std::max(0.f, cardH - 2.f * pad)};
      break;
    case PopoverPlacement::Above:
      cardH = std::max(0.f, th - ah);
      cardRect = {0.f, 0.f, tw, cardH};
      contentFrame = {pad, pad, std::max(0.f, tw - 2.f * pad), std::max(0.f, cardH - 2.f * pad)};
      break;
    case PopoverPlacement::End:
      cardW = std::max(0.f, tw - ah);
      cardH = th;
      cardRect = {ah, 0.f, cardW, cardH};
      contentFrame = {ah + pad, pad, std::max(0.f, cardW - 2.f * pad), std::max(0.f, cardH - 2.f * pad)};
      break;
    case PopoverPlacement::Start:
      cardW = std::max(0.f, tw - ah);
      cardH = th;
      cardRect = {0.f, 0.f, cardW, cardH};
      contentFrame = {pad, pad, std::max(0.f, cardW - 2.f * pad), std::max(0.f, cardH - 2.f * pad)};
      break;
    }
  }

  Path path = buildPopoverCalloutPath(value.placement, value.cornerRadius, value.arrow, PopoverCalloutShape::kArrowW,
                                      ah, cardRect, {tw, th});

  float const bw = std::max(1.f, value.borderWidth);
  Element pathEl = Element{PathShape{
      .path = std::move(path),
      .fill = FillStyle::solid(value.backgroundColor),
      .stroke = StrokeStyle::solid(value.borderColor, bw),
  }};

  ctx.pushChildIndex();
  if (StateStore* store = StateStore::current()) {
    store->resetSlotCursors();
  }
  ctx.rewindChildKeyIndex();

  // PathNode is not hit-tested (see HitTester); a full-bounds rect absorbs pointer/hover/scroll so
  // content below the overlay does not receive input in padding and non-rect areas.
  NodeId const blockId = ctx.graph().addRect(ctx.parentLayer(), RectNode{
      .bounds = {0.f, 0.f, tw, th},
      .fill = FillStyle::none(),
      .stroke = StrokeStyle::none(),
  });
  ctx.eventMap().insert(blockId, EventHandlers{
      .stableTargetKey = {},
      .onScroll = [](Vec2) {},
      .cursor = Cursor::Arrow,
  });

  LayoutConstraints innerForBuild{};
  innerForBuild.maxWidth = tw;
  innerForBuild.maxHeight = th;

  le.setChildFrame(Rect{0.f, 0.f, tw, th});
  ctx.pushConstraints(innerForBuild);
  pathEl.build(ctx);
  ctx.popConstraints();

  LayoutConstraints contentCs{};
  contentCs.maxWidth = contentFrame.width;
  contentCs.maxHeight = contentFrame.height;
  le.setChildFrame(contentFrame);
  ctx.pushConstraints(contentCs);
  value.content.build(ctx);
  ctx.popConstraints();

  ctx.popChildIndex();
  ctx.popLayer();
}

} // namespace flux
