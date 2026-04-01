#include <Flux/UI/Element.hpp>

#include <Flux/UI/Detail/LayoutDebugDump.hpp>
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

#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>

namespace flux {

namespace {

TextLayoutOptions textViewLayoutOptions(Text const& v, LayoutConstraints const&,
                                        LayoutHints const& h) {
  TextLayoutOptions o{};
  o.horizontalAlignment = v.horizontalAlignment;
  o.verticalAlignment = v.verticalAlignment;
  o.wrapping = v.wrapping;
  o.lineHeight = v.lineHeight;
  o.maxLines = v.maxLines;
  o.firstBaselineOffset = v.firstBaselineOffset;
  if (h.vStackCrossAlign) {
    o.horizontalAlignment = *h.vStackCrossAlign;
  }
  return o;
}

Rect explicitLeafBox(Rectangle const& v) {
  return {v.offsetX, v.offsetY, v.width, v.height};
}

Rect explicitLeafBox(Text const& v) {
  return {v.offsetX, v.offsetY, v.width, v.height};
}

Rect explicitLeafBox(views::Image const& v) {
  return {v.offsetX, v.offsetY, v.width, v.height};
}

struct PaddingModifier {
  float amount{};
  Element child;
  Element body() const {
    return Element{VStack{.padding = amount, .children = {child}}};
  }
};

struct BackgroundModifier {
  FillStyle fill;
  Element child;
  Element body() const {
    return Element{ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children =
            {
                Element{Rectangle{.width = 0.f, .height = 0.f, .fill = fill}},
                child,
            },
    }};
  }
};

struct FrameModifier {
  float w{};
  float h{};
  Element child;
  Element body() const {
    return Element{ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children =
            {
                Element{Rectangle{.width = w, .height = h, .fill = FillStyle::none()}},
                child,
            },
    }};
  }
};

struct BorderModifier {
  StrokeStyle stroke;
  Element child;
  Element body() const {
    return Element{ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children =
            {
                Element{Rectangle{
                    .width = 0.f,
                    .height = 0.f,
                    .fill = FillStyle::none(),
                    .stroke = stroke,
                }},
                child,
            },
    }};
  }
};

struct CornerModifier {
  CornerRadius radius{};
  Element child;
  Element body() const {
    return Element{ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children =
            {
                Element{Rectangle{
                    .cornerRadius = radius,
                    .fill = FillStyle::none(),
                }},
                child,
            },
    }};
  }
};

struct ClipModifier {
  bool clip{};
  Element child;
  Element body() const {
    return Element{ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .clip = clip,
        .children = {child},
    }};
  }
};

struct OverlayModifier {
  Element under;
  Element over;
  Element body() const {
    return Element{ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children = {under, over},
    }};
  }
};

struct TapModifier {
  std::function<void()> onTap;
  Element child;
  Element body() const {
    return Element{ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children =
            {
                child,
                Element{Rectangle{
                    .width = 0.f,
                    .height = 0.f,
                    .fill = FillStyle::none(),
                    .onTap = onTap,
                }},
            },
    }};
  }
};

} // namespace

void Element::build(BuildContext& ctx) const {
  layoutDebugPushElementBuild(measureId_);
  if (envLayer_) {
    EnvironmentStack::current().push(*envLayer_);
  }
  impl_->build(ctx);
  if (envLayer_) {
    EnvironmentStack::current().pop();
  }
  layoutDebugPopElementBuild();
}

Size Element::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                      LayoutHints const& hints, TextSystem& textSystem) const {
  if (envLayer_) {
    EnvironmentStack::current().push(*envLayer_);
  }
  Size sz{};
  // `measureCache` is keyed by measureId + constraints (see MeasureCache.hpp), not by leaf content.
  MeasureCache* const mc = envLayer_ ? nullptr : ctx.measureCache();
  if (mc && impl_->canMemoizeMeasure()) {
    MeasureCacheKey const key = makeMeasureCacheKey(measureId_, constraints, hints);
    if (std::optional<Size> const cached = mc->tryGet(key)) {
      ctx.advanceChildSlot();
      sz = *cached;
    } else {
      sz = impl_->measure(ctx, constraints, hints, textSystem);
      mc->put(key, sz);
    }
  } else {
    sz = impl_->measure(ctx, constraints, hints, textSystem);
  }
  if (envLayer_) {
    EnvironmentStack::current().pop();
  }
  layoutDebugRecordMeasure(measureId_, constraints, sz);
#ifndef NDEBUG
  assert(std::isfinite(sz.width) && std::isfinite(sz.height));
  assert(sz.width >= 0.f && sz.height >= 0.f);
#endif
  return sz;
}

namespace detail {

std::uint64_t nextElementMeasureId() {
  static std::uint64_t n = 1;
  return n++;
}

Popover* popoverOverlayStateIf(Element& el) {
  auto* m = dynamic_cast<Element::Model<Popover>*>(el.impl_.get());
  return m ? &m->value : nullptr;
}

} // namespace detail

Element::Element(Element const& other)
    : impl_(other.impl_ ? other.impl_->clone() : nullptr)
    , flexGrowOverride_(other.flexGrowOverride_)
    , flexShrinkOverride_(other.flexShrinkOverride_)
    , minMainSizeOverride_(other.minMainSizeOverride_)
    , envLayer_(other.envLayer_)
    , measureId_(detail::nextElementMeasureId()) {}

Element& Element::operator=(Element const& other) {
  if (this != &other) {
    impl_ = other.impl_ ? other.impl_->clone() : nullptr;
    flexGrowOverride_ = other.flexGrowOverride_;
    flexShrinkOverride_ = other.flexShrinkOverride_;
    minMainSizeOverride_ = other.minMainSizeOverride_;
    envLayer_ = other.envLayer_;
    measureId_ = detail::nextElementMeasureId();
  }
  return *this;
}

float Element::flexGrow() const {
  return flexGrowOverride_.value_or(impl_->flexGrow());
}

float Element::flexShrink() const {
  return flexShrinkOverride_.value_or(impl_->flexShrink());
}

float Element::minMainSize() const {
  return minMainSizeOverride_.value_or(impl_->minMainSize());
}

Element Element::withFlex(float grow, float shrink, float minMain) && {
  flexGrowOverride_ = grow;
  flexShrinkOverride_ = shrink;
  minMainSizeOverride_ = minMain;
  return std::move(*this);
}

void Element::Model<Rectangle>::build(BuildContext& ctx) const {
  ComponentKey const stableKey = ctx.leafComponentKey();
  ctx.advanceChildSlot();
  Rect const bounds = flux::detail::resolveLeafLayoutBounds(
      explicitLeafBox(value), ctx.layoutEngine().consumeAssignedFrame(), ctx.constraints(), ctx.hints(), true);
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
  layoutDebugLogLeaf("Rectangle", ctx.constraints(), bounds, detail::flexGrowOf(value),
                     detail::flexShrinkOf(value), detail::minMainSizeOf(value));
}

Size Element::Model<Rectangle>::measure(BuildContext& ctx, LayoutConstraints const& c, LayoutHints const&,
                                          TextSystem&) const {
  ctx.advanceChildSlot();
  if (value.width > 0.f || value.height > 0.f) {
    return {value.width, value.height};
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
  Rect const cf = ctx.layoutEngine().consumeAssignedFrame();
  if (cf.width > 0.f || cf.height > 0.f) {
    origin = {cf.x, cf.y};
  }
  ctx.graph().addText(ctx.parentLayer(), TextNode{
      .layout = value.layout,
      .origin = origin,
  });
  layoutDebugLogLeaf("LaidOutText", ctx.constraints(), cf, detail::flexGrowOf(value),
                     detail::flexShrinkOf(value), detail::minMainSizeOf(value));
}

Size Element::Model<LaidOutText>::measure(BuildContext& ctx, LayoutConstraints const&, LayoutHints const&,
                                          TextSystem&) const {
  ctx.advanceChildSlot();
  if (!value.layout) {
    return {};
  }
  return value.layout->measuredSize;
}

void Element::Model<Text>::build(BuildContext& ctx) const {
  ComponentKey const stableKey = ctx.leafComponentKey();
  ctx.advanceChildSlot();
  Rect const bounds = flux::detail::resolveLeafLayoutBounds(
      explicitLeafBox(value), ctx.layoutEngine().consumeAssignedFrame(), ctx.constraints(), ctx.hints(), false);
  assert(value.text.empty() || (bounds.width > 0.f && bounds.height > 0.f));

  float const pad = std::max(0.f, value.padding);
  Rect inner{bounds.x + pad, bounds.y + pad, std::max(0.f, bounds.width - 2.f * pad),
             std::max(0.f, bounds.height - 2.f * pad)};

  std::shared_ptr<TextLayout> layout;
  if (!value.text.empty()) {
    TextLayoutOptions const opts = textViewLayoutOptions(value, ctx.constraints(), ctx.hints());
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
  layoutDebugLogLeaf("Text", ctx.constraints(), bounds, detail::flexGrowOf(value),
                     detail::flexShrinkOf(value), detail::minMainSizeOf(value));
}

Size Element::Model<Text>::measure(BuildContext& ctx, LayoutConstraints const& c, LayoutHints const& hints,
                                     TextSystem& ts) const {
  ctx.advanceChildSlot();
  float const pad = value.padding * 2.f;
  TextLayoutOptions const opts = textViewLayoutOptions(value, c, hints);

  // Explicit box: both dimensions fixed.
  if (value.width > 0.f && value.height > 0.f) {
    return {value.width, value.height};
  }
  // Fixed height, width from constraints / text (e.g. animated row height with width TBD).
  if (value.width <= 0.f && value.height > 0.f) {
    float const mw = std::isfinite(c.maxWidth) && c.maxWidth > pad ? c.maxWidth - pad : 0.f;
    Size const s = ts.measure(value.text, value.font, value.color, mw, opts);
    float w = s.width + pad;
    if (std::isfinite(c.maxWidth) && c.maxWidth > 0.f) {
      w = std::min(w, c.maxWidth);
    }
    return {w, value.height};
  }
  // Fixed width, height from text measurement.
  if (value.width > 0.f && value.height <= 0.f) {
    float const mw = std::max(0.f, value.width - pad);
    Size const s = ts.measure(value.text, value.font, value.color, mw, opts);
    float h = s.height + pad;
    if (std::isfinite(c.maxHeight) && c.maxHeight > 0.f) {
      h = std::min(h, c.maxHeight);
    }
    return {value.width, h};
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
  Rect const bounds = flux::detail::resolveLeafLayoutBounds(
      explicitLeafBox(value), ctx.layoutEngine().consumeAssignedFrame(), ctx.constraints(), ctx.hints(), false);
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
  layoutDebugLogLeaf("Image", ctx.constraints(), bounds, detail::flexGrowOf(value),
                     detail::flexShrinkOf(value), detail::minMainSizeOf(value));
}

Size Element::Model<views::Image>::measure(BuildContext& ctx, LayoutConstraints const& c, LayoutHints const&,
                                             TextSystem&) const {
  ctx.advanceChildSlot();
  if (value.width > 0.f || value.height > 0.f) {
    return {value.width, value.height};
  }
  float const w = std::isfinite(c.maxWidth) ? c.maxWidth : 0.f;
  float const h = std::isfinite(c.maxHeight) ? c.maxHeight : 0.f;
  return {w, h};
}

void Element::Model<PathShape>::build(BuildContext& ctx) const {
  ctx.advanceChildSlot();
  PathNode node{.path = value.path, .fill = value.fill, .stroke = value.stroke};
  Rect const cf = ctx.layoutEngine().consumeAssignedFrame();
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
  layoutDebugLogLeaf("PathShape", ctx.constraints(), cf, detail::flexGrowOf(value),
                     detail::flexShrinkOf(value), detail::minMainSizeOf(value));
}

Size Element::Model<PathShape>::measure(BuildContext& ctx, LayoutConstraints const&, LayoutHints const&,
                                        TextSystem&) const {
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
  float const minX = std::min(value.from.x, value.to.x);
  float const maxX = std::max(value.from.x, value.to.x);
  float const minY = std::min(value.from.y, value.to.y);
  float const maxY = std::max(value.from.y, value.to.y);
  Rect const lineBounds{minX, minY, maxX - minX, maxY - minY};
  layoutDebugLogLeaf("Line", ctx.constraints(), lineBounds, detail::flexGrowOf(value),
                     detail::flexShrinkOf(value), detail::minMainSizeOf(value));
}

Size Element::Model<Line>::measure(BuildContext& ctx, LayoutConstraints const&, LayoutHints const&,
                                   TextSystem&) const {
  ctx.advanceChildSlot();
  float const minX = std::min(value.from.x, value.to.x);
  float const maxX = std::max(value.from.x, value.to.x);
  float const minY = std::min(value.from.y, value.to.y);
  float const maxY = std::max(value.from.y, value.to.y);
  return {maxX - minX, maxY - minY};
}

Element Element::padding(float all) && {
  return Element{PaddingModifier{all, std::move(*this)}};
}

Element Element::background(FillStyle fill) && {
  return Element{BackgroundModifier{std::move(fill), std::move(*this)}};
}

Element Element::frame(float width, float height) && {
  if (width <= 0.f && height <= 0.f) {
    return std::move(*this);
  }
  return Element{FrameModifier{width, height, std::move(*this)}};
}

Element Element::border(StrokeStyle stroke) && {
  return Element{BorderModifier{std::move(stroke), std::move(*this)}};
}

Element Element::cornerRadius(CornerRadius radius) && {
  return Element{CornerModifier{radius, std::move(*this)}};
}

Element Element::opacity(float opacity) && {
  return Element{LayerEffect{.opacity = opacity, .child = std::move(*this)}};
}

Element Element::offset(Vec2 delta) && {
  return Element{LayerEffect{.offset = delta, .child = std::move(*this)}};
}

Element Element::offset(float dx, float dy) && {
  return Element{LayerEffect{.offset = {dx, dy}, .child = std::move(*this)}};
}

Element Element::clipContent(bool clip) && {
  return Element{ClipModifier{clip, std::move(*this)}};
}

Element Element::overlay(Element over) && {
  return Element{OverlayModifier{std::move(*this), std::move(over)}};
}

Element Element::onTapGesture(std::function<void()> handler) && {
  return Element{TapModifier{std::move(handler), std::move(*this)}};
}

} // namespace flux
