#include <Flux/UI/Element.hpp>

#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/LayoutTree.hpp>
#include <Flux/UI/RenderContext.hpp>
#include <Flux/UI/Views/PopoverCalloutShape.hpp>
#include <Flux/UI/Detail/LayoutDebugDump.hpp>
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

Size PopoverCalloutShape::measure(LayoutContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                                  TextSystem& ts) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutConstraints cc = innerConstraintsForPopoverContent(*this, constraints);

  ctx.pushChildIndex();
  Size const inner = content.measure(ctx, cc, LayoutHints{}, ts);
  ctx.popChildIndex();

  float const pad = padding;
  float const ah = PopoverCalloutShape::kArrowH;
  float const awTri = PopoverCalloutShape::kArrowW;

  float const cardW = inner.width + 2.f * pad;
  float const cardH = inner.height + 2.f * pad;

  if (!arrow) {
    return {cardW, cardH};
  }
  switch (placement) {
  case PopoverPlacement::Below:
  case PopoverPlacement::Above:
    return {cardW, cardH + ah};
  case PopoverPlacement::End:
  case PopoverPlacement::Start:
    return {cardW + ah, std::max(cardH, awTri)};
  }
  return {cardW, cardH};
}

void PopoverCalloutShape::layout(LayoutContext& ctx) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine& le = ctx.layoutEngine();
  Rect const parentFrame = le.consumeAssignedFrame();
  LayoutConstraints outer = ctx.constraints();
  if (maxSize) {
    if (std::isfinite(maxSize->width) && maxSize->width > 0.f) {
      outer.maxWidth = std::min(outer.maxWidth, maxSize->width);
    }
    if (std::isfinite(maxSize->height) && maxSize->height > 0.f) {
      outer.maxHeight = std::min(outer.maxHeight, maxSize->height);
    }
  }
  layoutDebugLogContainer("PopoverCalloutShape", outer, parentFrame);

  LayoutConstraints const ccInner = innerConstraintsForPopoverContent(*this, outer);
  ctx.pushChildIndex();
  Size const innerMeasured = content.measure(ctx, ccInner, LayoutHints{}, ctx.textSystem());
  ctx.popChildIndex();

  float const assignedW = assignedSpan(parentFrame.width, outer.maxWidth);
  float const assignedH = assignedSpan(parentFrame.height, outer.maxHeight);

  float const pad = padding;
  float const ah = PopoverCalloutShape::kArrowH;

  float tw = std::max(0.f, assignedW);
  float th = std::max(0.f, assignedH);

  float const cardHFromContent = innerMeasured.height + 2.f * pad;
  if (!arrow) {
    if (cardHFromContent > 1e-3f && cardHFromContent < th) {
      th = cardHFromContent;
    }
  } else {
    switch (placement) {
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

  if (parentFrame.width > 0.f || parentFrame.height > 0.f) {
    ctx.pushLayerWorldTransform(Mat3::translate(parentFrame.x, parentFrame.y));
  }

  LayoutNode shell{};
  shell.kind = LayoutNode::Kind::Container;
  shell.frame = Rect{0.f, 0.f, tw, th};
  shell.constraints = outer;
  shell.containerSpec.kind = ContainerLayerSpec::Kind::Standard;
  shell.containerTag = LayoutNode::ContainerTag::PopoverCalloutShape;
  shell.element = ctx.currentElement();
  shell.hints = ctx.hints();
  LayoutNodeId const shellId = ctx.pushLayoutNode(std::move(shell));
  ctx.registerCompositeSubtreeRootIfPending(shellId);
  ctx.pushLayoutParent(shellId);

  float cardW = tw;
  float cardH = th;
  Rect cardRect{};
  Rect contentFrame{};

  if (!arrow) {
    cardRect = {0.f, 0.f, tw, th};
    contentFrame = {pad, pad, std::max(0.f, tw - 2.f * pad), std::max(0.f, th - 2.f * pad)};
  } else {
    switch (placement) {
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

  Path path = buildPopoverCalloutPath(placement, cornerRadius, arrow, PopoverCalloutShape::kArrowW,
                                      ah, cardRect, {tw, th});

  float const bw = std::max(1.f, borderWidth);
  Element& pathEl = ctx.pinElement(Element{PathShape{
      .path = std::move(path),
      .fill = FillStyle::solid(backgroundColor),
      .stroke = StrokeStyle::solid(borderColor, bw),
      .shadow = shadow,
  }});

  ctx.pushChildIndex();
  if (StateStore* store = StateStore::current()) {
    store->resetSlotCursors();
  }
  ctx.rewindChildKeyIndex();

  LayoutConstraints innerForBuild{};
  innerForBuild.maxWidth = tw;
  innerForBuild.maxHeight = th;

  le.setChildFrame(Rect{0.f, 0.f, tw, th});
  ctx.pushConstraints(innerForBuild);
  pathEl.layout(ctx);
  ctx.popConstraints();

  LayoutConstraints contentCs{};
  contentCs.maxWidth = contentFrame.width;
  contentCs.maxHeight = contentFrame.height;
  le.setChildFrame(contentFrame);
  ctx.pushConstraints(contentCs);
  content.layout(ctx);
  ctx.popConstraints();

  ctx.popChildIndex();
  ctx.popLayoutParent();
  if (parentFrame.width > 0.f || parentFrame.height > 0.f) {
    ctx.popLayerWorldTransform();
  }
}

void PopoverCalloutShape::renderFromLayout(RenderContext&, LayoutNode const&) const {}

} // namespace flux
