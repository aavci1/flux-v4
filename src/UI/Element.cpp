#include <Flux/UI/Element.hpp>

#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/Graphics/AttributedString.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include <algorithm>
#include <cmath>

namespace flux {

namespace {

/// Resolves axis-aligned bounds for leaves that use an optional `frame`, then `childFrame` from layout,
/// then the current `LayoutConstraints` (e.g. root view under `body()` has no `childFrame` until this).
Rect resolveLeafBounds(Rect const& frame, Rect const& childFrame, LayoutConstraints const& constraints) {
  Rect bounds = frame;
  if (childFrame.width > 0.f || childFrame.height > 0.f) {
    bounds = childFrame;
  }
  if (bounds.width <= 0.f || bounds.height <= 0.f) {
    float const w = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
    float const h = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
    if (w > 0.f && h > 0.f) {
      bounds = Rect{0, 0, w, h};
    }
  }
  return bounds;
}

TextAttribute textViewAttribute(Text const& v) {
  TextAttribute a{};
  a.fontFamily = v.fontFamily;
  a.fontSize = v.fontSize;
  a.fontWeight = v.fontWeight;
  a.italic = v.italic;
  a.color = v.color;
  return a;
}

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

Size Element::measure(LayoutConstraints const& constraints, TextSystem& textSystem) const {
  return impl_->measure(constraints, textSystem);
}

bool Element::isSpacer() const { return impl_->isSpacer(); }

void Element::Model<Rectangle>::build(BuildContext& ctx) const {
  Rect const bounds =
      resolveLeafBounds(value.frame, ctx.layoutEngine().childFrame(), ctx.constraints());
  NodeId const id = ctx.graph().addRect(ctx.parentLayer(), RectNode{
      .bounds = bounds,
      .cornerRadius = value.cornerRadius,
      .fill = value.fill,
      .stroke = value.stroke,
  });
  if (value.onTap || value.onPointerDown || value.onPointerUp || value.onPointerMove) {
    ctx.eventMap().insert(id, EventHandlers{
        .onTap = value.onTap,
        .onPointerDown = value.onPointerDown,
        .onPointerUp = value.onPointerUp,
        .onPointerMove = value.onPointerMove,
    });
  }
}

Size Element::Model<Rectangle>::measure(LayoutConstraints const& c, TextSystem&) const {
  if (value.frame.width > 0.f || value.frame.height > 0.f) {
    return {value.frame.width, value.frame.height};
  }
  float const w = std::isfinite(c.maxWidth) ? c.maxWidth : 0.f;
  float const h = std::isfinite(c.maxHeight) ? c.maxHeight : 0.f;
  return {w, h};
}

void Element::Model<LaidOutText>::build(BuildContext& ctx) const {
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

Size Element::Model<LaidOutText>::measure(LayoutConstraints const&, TextSystem&) const {
  if (!value.layout) {
    return {};
  }
  return value.layout->measuredSize;
}

void Element::Model<Text>::build(BuildContext& ctx) const {
  Rect const bounds =
      resolveLeafBounds(value.frame, ctx.layoutEngine().childFrame(), ctx.constraints());

  float const pad = std::max(0.f, value.padding);
  Rect inner{bounds.x + pad, bounds.y + pad, std::max(0.f, bounds.width - 2.f * pad),
             std::max(0.f, bounds.height - 2.f * pad)};

  std::shared_ptr<TextLayout> layout;
  if (!value.text.empty()) {
    TextAttribute const attr = textViewAttribute(value);
    TextLayoutOptions const opts = textViewLayoutOptions(value);
    layout = ctx.textSystem().layout(value.text, attr, inner, opts);
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

Size Element::Model<Text>::measure(LayoutConstraints const& c, TextSystem& ts) const {
  if (value.frame.width > 0.f || value.frame.height > 0.f) {
    return {value.frame.width, value.frame.height};
  }
  float const mw = std::isfinite(c.maxWidth) ? c.maxWidth : 0.f;
  TextAttribute const attr = textViewAttribute(value);
  TextLayoutOptions const opts = textViewLayoutOptions(value);
  Size s = ts.measure(value.text, attr, mw, opts);
  float const p = value.padding * 2.f;
  float w = s.width + p;
  float h = s.height + p;
  if (std::isfinite(c.maxWidth) && c.maxWidth > 0.f) {
    w = std::min(w, c.maxWidth);
  }
  if (std::isfinite(c.maxHeight) && c.maxHeight > 0.f) {
    h = std::min(h, c.maxHeight);
  }
  return {w, h};
}

void Element::Model<views::Image>::build(BuildContext& ctx) const {
  if (!value.source) {
    return;
  }
  Rect const bounds =
      resolveLeafBounds(value.frame, ctx.layoutEngine().childFrame(), ctx.constraints());
  NodeId const id = ctx.graph().addImage(ctx.parentLayer(), ImageNode{
      .image = value.source,
      .bounds = bounds,
      .fillMode = value.fillMode,
      .cornerRadius = value.cornerRadius,
      .opacity = value.opacity,
  });
  if (value.onTap) {
    ctx.eventMap().insert(id, EventHandlers{.onTap = value.onTap});
  }
}

Size Element::Model<views::Image>::measure(LayoutConstraints const& c, TextSystem&) const {
  if (value.frame.width > 0.f || value.frame.height > 0.f) {
    return {value.frame.width, value.frame.height};
  }
  float const w = std::isfinite(c.maxWidth) ? c.maxWidth : 0.f;
  float const h = std::isfinite(c.maxHeight) ? c.maxHeight : 0.f;
  return {w, h};
}

void Element::Model<PathShape>::build(BuildContext& ctx) const {
  ctx.graph().addPath(ctx.parentLayer(), PathNode{
      .path = value.path,
      .fill = value.fill,
      .stroke = value.stroke,
  });
}

Size Element::Model<PathShape>::measure(LayoutConstraints const&, TextSystem&) const {
  Rect const b = value.path.getBounds();
  return {b.width, b.height};
}

void Element::Model<Line>::build(BuildContext& ctx) const {
  ctx.graph().addLine(ctx.parentLayer(), LineNode{
      .from = value.from,
      .to = value.to,
      .stroke = value.stroke,
  });
}

Size Element::Model<Line>::measure(LayoutConstraints const&, TextSystem&) const {
  float const minX = std::min(value.from.x, value.to.x);
  float const maxX = std::max(value.from.x, value.to.x);
  float const minY = std::min(value.from.y, value.to.y);
  float const maxY = std::max(value.from.y, value.to.y);
  return {maxX - minX, maxY - minY};
}

} // namespace flux
