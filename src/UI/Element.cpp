#include <Flux/UI/Element.hpp>

#include <Flux/UI/Environment.hpp>
#include <Flux/UI/Views/Popover.hpp>

#include <Flux/Core/Cursor.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/MeasureCache.hpp>
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

TextLayoutOptions textViewLayoutOptions(Text const& v, LayoutConstraints const& c) {
  TextLayoutOptions o{};
  o.horizontalAlignment = v.horizontalAlignment;
  o.verticalAlignment = v.verticalAlignment;
  o.wrapping = v.wrapping;
  o.lineHeight = v.lineHeight;
  o.maxLines = v.maxLines;
  o.firstBaselineOffset = v.firstBaselineOffset;
  if (c.vStackCrossAlign) {
    o.horizontalAlignment = *c.vStackCrossAlign;
  }
  return o;
}

} // namespace

void Element::build(BuildContext& ctx) const {
  if (envLayer_) {
    EnvironmentStack::current().push(*envLayer_);
  }
  impl_->build(ctx);
  if (envLayer_) {
    EnvironmentStack::current().pop();
  }
}

Size Element::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                      TextSystem& textSystem) const {
  if (envLayer_) {
    EnvironmentStack::current().push(*envLayer_);
  }
  Size sz{};
  // `measureCache` is keyed by measureId + constraints (see MeasureCache.hpp), not by leaf content.
  MeasureCache* const mc = envLayer_ ? nullptr : ctx.measureCache();
  if (mc && impl_->canMemoizeMeasure()) {
    MeasureCacheKey const key = makeMeasureCacheKey(measureId_, constraints);
    if (std::optional<Size> const cached = mc->tryGet(key)) {
      ctx.advanceChildSlot();
      sz = *cached;
    } else {
      sz = impl_->measure(ctx, constraints, textSystem);
      mc->put(key, sz);
    }
  } else {
    sz = impl_->measure(ctx, constraints, textSystem);
  }
  if (envLayer_) {
    EnvironmentStack::current().pop();
  }
  return sz;
}

namespace detail {

Popover* popoverOverlayStateIf(Element& el) {
  auto* m = dynamic_cast<Element::Model<Popover>*>(el.impl_.get());
  return m ? &m->value : nullptr;
}

} // namespace detail

void Element::Model<Rectangle>::build(BuildContext& ctx) const {
  ComponentKey const stableKey = ctx.leafComponentKey();
  ctx.advanceChildSlot();
  Rect const bounds = flux::detail::resolveRectangleBounds(
      value.frame, ctx.layoutEngine().childFrame(), ctx.constraints());
  NodeId const id = ctx.graph().addRect(ctx.parentLayer(), RectNode{
      .bounds = bounds,
      .cornerRadius = value.cornerRadius,
      .fill = value.fill,
      .stroke = value.stroke,
  });
  bool const focusable = value.focusable || static_cast<bool>(value.onKeyDown) ||
                         static_cast<bool>(value.onKeyUp) || static_cast<bool>(value.onTextInput);
  if (value.onTap || value.onPointerDown || value.onPointerUp || value.onPointerMove || value.onScroll ||
      value.onKeyDown || value.onKeyUp || value.onTextInput || value.focusable ||
      value.cursor != Cursor::Inherit || value.cursorPassthrough) {
    ctx.eventMap().insert(id, EventHandlers{
        .stableTargetKey = stableKey,
        .onTap = value.onTap,
        .onPointerDown = value.onPointerDown,
        .onPointerUp = value.onPointerUp,
        .onPointerMove = value.onPointerMove,
        .onScroll = value.onScroll,
        .onKeyDown = value.onKeyDown,
        .onKeyUp = value.onKeyUp,
        .onTextInput = value.onTextInput,
        .focusable = focusable,
        .cursor = value.cursor,
        .cursorPassthrough = value.cursorPassthrough,
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
    TextLayoutOptions const opts = textViewLayoutOptions(value, ctx.constraints());
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
    bool const focusable = value.focusable || static_cast<bool>(value.onKeyDown) ||
                           static_cast<bool>(value.onKeyUp) || static_cast<bool>(value.onTextInput);
    if (value.onTap || value.onPointerDown || value.onPointerUp || value.onPointerMove || value.onKeyDown ||
        value.onKeyUp || value.onTextInput || value.focusable || value.cursor != Cursor::Inherit) {
      ctx.eventMap().insert(id, EventHandlers{
          .stableTargetKey = stableKey,
          .onTap = value.onTap,
          .onPointerDown = value.onPointerDown,
          .onPointerUp = value.onPointerUp,
          .onPointerMove = value.onPointerMove,
          .onKeyDown = value.onKeyDown,
          .onKeyUp = value.onKeyUp,
          .onTextInput = value.onTextInput,
          .focusable = focusable,
          .cursor = value.cursor,
      });
    }
  }

  if (layout && !layout->runs.empty()) {
    NodeId const textId = ctx.graph().addText(ctx.parentLayer(), TextNode{
        .layout = layout,
        .origin = {inner.x, inner.y},
        .allocation = inner,
    });
    bool const textFocusable = value.focusable || static_cast<bool>(value.onKeyDown) ||
                               static_cast<bool>(value.onKeyUp) || static_cast<bool>(value.onTextInput);
    if (value.onTap || value.onPointerDown || value.onPointerUp || value.onPointerMove || value.onKeyDown ||
        value.onKeyUp || value.onTextInput || value.focusable || value.cursor != Cursor::Inherit) {
      ctx.eventMap().insert(textId, EventHandlers{
          .stableTargetKey = stableKey,
          .onTap = value.onTap,
          .onPointerDown = value.onPointerDown,
          .onPointerUp = value.onPointerUp,
          .onPointerMove = value.onPointerMove,
          .onKeyDown = value.onKeyDown,
          .onKeyUp = value.onKeyUp,
          .onTextInput = value.onTextInput,
          .focusable = textFocusable,
          .cursor = value.cursor,
      });
    }
  }
}

Size Element::Model<Text>::measure(BuildContext& ctx, LayoutConstraints const& c, TextSystem& ts) const {
  ctx.advanceChildSlot();
  float const pad = value.padding * 2.f;
  TextLayoutOptions const opts = textViewLayoutOptions(value, c);

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
  PathNode node{.path = value.path, .fill = value.fill, .stroke = value.stroke};
  Rect const cf = ctx.layoutEngine().childFrame();
  Rect const pb = value.path.getBounds();
  float dx = 0.f;
  float dy = 0.f;
  if (cf.width > pb.width + 1e-6f) {
    dx = (cf.width - pb.width) * 0.5f - pb.x;
  }
  if (cf.height > pb.height + 1e-6f) {
    dy = (cf.height - pb.height) * 0.5f - pb.y;
  }
  float const tx = cf.x + dx;
  float const ty = cf.y + dy;
  if (tx != 0.f || ty != 0.f) {
    LayerNode layer{};
    layer.transform = Mat3::translate(tx, ty);
    NodeId const lid = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
    ctx.graph().addPath(lid, std::move(node));
  } else {
    ctx.graph().addPath(ctx.parentLayer(), std::move(node));
  }
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
