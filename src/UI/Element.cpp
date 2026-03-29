#include <Flux/UI/Element.hpp>

#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Detail/LeafBounds.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>

namespace flux {

namespace {

TextLayoutOptions textViewLayoutOptions(Text const& v) {
  TextLayoutOptions o{};
  o.horizontalAlignment = v.horizontalAlignment;
  o.verticalAlignment = v.verticalAlignment;
  o.wrapping = v.wrapping;
  o.lineHeight = v.lineHeight;
  o.maxLines = v.maxLines;
  o.firstBaselineOffset = v.firstBaselineOffset;
  return o;
}

} // namespace

void Element::build(BuildContext& ctx) const { impl_->build(ctx); }

Size Element::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                      TextSystem& textSystem) const {
  return impl_->measure(ctx, constraints, textSystem);
}

void Element::Model<Rectangle>::build(BuildContext& ctx) const {
  ComponentKey const stableKey = ctx.leafComponentKey();
  ctx.advanceChildSlot();
  Rect const bounds = flux::detail::resolveLeafBounds(
      value.frame, ctx.layoutEngine().childFrame(), ctx.constraints());
  NodeId const id = ctx.graph().addRect(ctx.parentLayer(), RectNode{
      .bounds = bounds,
      .cornerRadius = value.cornerRadius,
      .fill = value.fill,
      .stroke = value.stroke,
  });
  if (value.onTap || value.onPointerDown || value.onPointerUp || value.onPointerMove || value.onScroll) {
    ctx.eventMap().insert(id, EventHandlers{
        .stableTargetKey = stableKey,
        .onTap = value.onTap,
        .onPointerDown = value.onPointerDown,
        .onPointerUp = value.onPointerUp,
        .onPointerMove = value.onPointerMove,
        .onScroll = value.onScroll,
    });
  }
}

Size Element::Model<Rectangle>::measure(BuildContext& ctx, LayoutConstraints const& c, TextSystem&) const {
  ctx.advanceChildSlot();
  if (value.frame.width > 0.f || value.frame.height > 0.f) {
    return {value.frame.width, value.frame.height};
  }
  float const w = std::isfinite(c.maxWidth) ? c.maxWidth : 0.f;
  return {w, 0.f};
}

void Element::Model<LaidOutText>::build(BuildContext& ctx) const {
  ctx.advanceChildSlot();
  if (!value.layout) {
    return;
  }
  Point origin = value.origin;
  Rect const cf = ctx.layoutEngine().childFrame();
  if (cf.width > 0.f || cf.height > 0.f) {
    origin = {cf.x, cf.y};
  }
  ctx.graph().addText(ctx.parentLayer(), TextNode{
      .layout = value.layout,
      .origin = origin,
  });
}

Size Element::Model<LaidOutText>::measure(BuildContext& ctx, LayoutConstraints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  if (!value.layout) {
    return {};
  }
  return value.layout->measuredSize;
}

void Element::Model<Text>::build(BuildContext& ctx) const {
  ComponentKey const stableKey = ctx.leafComponentKey();
  ctx.advanceChildSlot();
  Rect const bounds = flux::detail::resolveLeafBounds(
      value.frame, ctx.layoutEngine().childFrame(), ctx.constraints());
  assert(value.text.empty() || (bounds.width > 0.f && bounds.height > 0.f));

  float const pad = std::max(0.f, value.padding);
  Rect inner{bounds.x + pad, bounds.y + pad, std::max(0.f, bounds.width - 2.f * pad),
             std::max(0.f, bounds.height - 2.f * pad)};

  std::shared_ptr<TextLayout> layout;
  if (!value.text.empty()) {
    TextLayoutOptions const opts = textViewLayoutOptions(value);
    layout = ctx.textSystem().layout(value.text, value.font, value.color, inner, opts);
  }

  bool const drawBackground = !value.background.isNone() || !value.border.isNone();
  if (drawBackground) {
    NodeId const id = ctx.graph().addRect(ctx.parentLayer(), RectNode{
        .bounds = bounds,
        .cornerRadius = value.cornerRadius,
        .fill = value.background,
        .stroke = value.border,
    });
    if (value.onTap || value.onPointerDown || value.onPointerUp || value.onPointerMove) {
      ctx.eventMap().insert(id, EventHandlers{
          .stableTargetKey = stableKey,
          .onTap = value.onTap,
          .onPointerDown = value.onPointerDown,
          .onPointerUp = value.onPointerUp,
          .onPointerMove = value.onPointerMove,
      });
    }
  }

  if (layout && !layout->runs.empty()) {
    ctx.graph().addText(ctx.parentLayer(), TextNode{
        .layout = layout,
        .origin = {inner.x, inner.y},
    });
  }
}

Size Element::Model<Text>::measure(BuildContext& ctx, LayoutConstraints const& c, TextSystem& ts) const {
  ctx.advanceChildSlot();
  float const pad = value.padding * 2.f;
  TextLayoutOptions const opts = textViewLayoutOptions(value);

  // Explicit box: both dimensions fixed.
  if (value.frame.width > 0.f && value.frame.height > 0.f) {
    return {value.frame.width, value.frame.height};
  }
  // Fixed height, width from constraints / text (e.g. animated row height with width TBD).
  if (value.frame.width <= 0.f && value.frame.height > 0.f) {
    float const mw = std::isfinite(c.maxWidth) && c.maxWidth > pad ? c.maxWidth - pad : 0.f;
    Size const s = ts.measure(value.text, value.font, value.color, mw, opts);
    float w = s.width + pad;
    if (std::isfinite(c.maxWidth) && c.maxWidth > 0.f) {
      w = std::min(w, c.maxWidth);
    }
    return {w, value.frame.height};
  }
  // Fixed width, height from text measurement.
  if (value.frame.width > 0.f && value.frame.height <= 0.f) {
    float const mw = std::max(0.f, value.frame.width - pad);
    Size const s = ts.measure(value.text, value.font, value.color, mw, opts);
    float h = s.height + pad;
    if (std::isfinite(c.maxHeight) && c.maxHeight > 0.f) {
      h = std::min(h, c.maxHeight);
    }
    return {value.frame.width, h};
  }

  float const mw = std::isfinite(c.maxWidth) && c.maxWidth > pad ? c.maxWidth - pad : 0.f;
  Size s = ts.measure(value.text, value.font, value.color, mw, opts);
  float w = s.width + pad;
  float h = s.height + pad;
  if (std::isfinite(c.maxWidth) && c.maxWidth > 0.f) {
    w = std::min(w, c.maxWidth);
  }
  if (std::isfinite(c.maxHeight) && c.maxHeight > 0.f) {
    h = std::min(h, c.maxHeight);
  }
  return {w, h};
}

void Element::Model<views::Image>::build(BuildContext& ctx) const {
  ComponentKey const stableKey = ctx.leafComponentKey();
  ctx.advanceChildSlot();
  if (!value.source) {
    return;
  }
  Rect const bounds = flux::detail::resolveLeafBounds(
      value.frame, ctx.layoutEngine().childFrame(), ctx.constraints());
  NodeId const id = ctx.graph().addImage(ctx.parentLayer(), ImageNode{
      .image = value.source,
      .bounds = bounds,
      .fillMode = value.fillMode,
      .cornerRadius = value.cornerRadius,
      .opacity = value.opacity,
  });
  if (value.onTap) {
    ctx.eventMap().insert(id, EventHandlers{.stableTargetKey = stableKey, .onTap = value.onTap});
  }
}

Size Element::Model<views::Image>::measure(BuildContext& ctx, LayoutConstraints const& c, TextSystem&) const {
  ctx.advanceChildSlot();
  if (value.frame.width > 0.f || value.frame.height > 0.f) {
    return {value.frame.width, value.frame.height};
  }
  float const w = std::isfinite(c.maxWidth) ? c.maxWidth : 0.f;
  float const h = std::isfinite(c.maxHeight) ? c.maxHeight : 0.f;
  return {w, h};
}

void Element::Model<PathShape>::build(BuildContext& ctx) const {
  ctx.advanceChildSlot();
  ctx.graph().addPath(ctx.parentLayer(), PathNode{
      .path = value.path,
      .fill = value.fill,
      .stroke = value.stroke,
  });
}

Size Element::Model<PathShape>::measure(BuildContext& ctx, LayoutConstraints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  Rect const b = value.path.getBounds();
  return {b.width, b.height};
}

void Element::Model<Line>::build(BuildContext& ctx) const {
  ctx.advanceChildSlot();
  ctx.graph().addLine(ctx.parentLayer(), LineNode{
      .from = value.from,
      .to = value.to,
      .stroke = value.stroke,
  });
}

Size Element::Model<Line>::measure(BuildContext& ctx, LayoutConstraints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  float const minX = std::min(value.from.x, value.to.x);
  float const maxX = std::max(value.from.x, value.to.x);
  float const minY = std::min(value.from.y, value.to.y);
  float const maxY = std::max(value.from.y, value.to.y);
  return {maxX - minX, maxY - minY};
}

} // namespace flux
