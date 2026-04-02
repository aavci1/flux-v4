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

#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace flux {

using namespace flux::layout;

ElementModifiers::ElementModifiers(ElementModifiers const& o)
    : padding(o.padding)
    , background(o.background)
    , border(o.border)
    , cornerRadius(o.cornerRadius)
    , opacity(o.opacity)
    , offset(o.offset)
    , clip(o.clip)
    , onTap(o.onTap)
    , frameWidth(o.frameWidth)
    , frameHeight(o.frameHeight)
    , overlay(o.overlay ? std::make_unique<Element>(*o.overlay) : nullptr) {}

ElementModifiers& ElementModifiers::operator=(ElementModifiers const& o) {
  if (this != &o) {
    padding = o.padding;
    background = o.background;
    border = o.border;
    cornerRadius = o.cornerRadius;
    opacity = o.opacity;
    offset = o.offset;
    clip = o.clip;
    onTap = o.onTap;
    frameWidth = o.frameWidth;
    frameHeight = o.frameHeight;
    overlay = o.overlay ? std::make_unique<Element>(*o.overlay) : nullptr;
  }
  return *this;
}

ElementModifiers::~ElementModifiers() = default;

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

Rect explicitLeafBox(Text const&) {
  return {};
}

Rect explicitLeafBox(views::Image const&) {
  return {};
}

} // namespace

void Element::build(BuildContext& ctx) const {
  layoutDebugPushElementBuild(measureId_);
  if (envLayer_) {
    EnvironmentStack::current().push(*envLayer_);
  }
  if (modifiers_ && modifiers_->needsModifierPass()) {
    buildWithModifiers(ctx);
  } else {
    impl_->build(ctx);
  }
  if (envLayer_) {
    EnvironmentStack::current().pop();
  }
  layoutDebugPopElementBuild();
}

void Element::buildWithModifiers(BuildContext& ctx) const {
  ElementModifiers const& m = *modifiers_;
  ComponentKey const stableKey = ctx.leafComponentKey();
  ContainerBuildScope scope(ctx);
  float const assignedW = assignedSpan(scope.parentFrame.width, scope.outer.maxWidth);
  float const assignedH = assignedSpan(scope.parentFrame.height, scope.outer.maxHeight);
  float outerW = std::max(0.f, assignedW);
  float outerH = std::max(0.f, assignedH);

  bool const needEffectLayer = m.opacity < 1.f - 1e-6f || std::fabs(m.offset.x) > 1e-6f ||
                               std::fabs(m.offset.y) > 1e-6f || m.clip;
  bool const needBg = !m.background.isNone() || !m.border.isNone() || !m.cornerRadius.isZero();

  if (needEffectLayer) {
    LayerNode layer{};
    layer.opacity = m.opacity;
    layer.transform =
        Mat3::translate(scope.parentFrame.x + m.offset.x, scope.parentFrame.y + m.offset.y);
    if (m.clip && outerW > 0.f && outerH > 0.f) {
      layer.clip = Rect{0.f, 0.f, outerW, outerH};
    }
    NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
    scope.pushCustomLayer(layerId);
  }

  Rect const absOuter = scope.parentFrame;
  Rect const localOuter{0.f, 0.f, outerW, outerH};
  Rect const bgBounds = needEffectLayer ? localOuter : absOuter;

  bool const needHitRect = needBg || static_cast<bool>(m.onTap);
  if (needHitRect) {
    FillStyle fill = FillStyle::none();
    StrokeStyle stroke = StrokeStyle::none();
    if (needBg) {
      fill = m.background;
      stroke = m.border;
    }
    NodeId const rid = ctx.graph().addRect(ctx.parentLayer(), RectNode{
        .bounds = bgBounds,
        .cornerRadius = m.cornerRadius,
        .fill = std::move(fill),
        .stroke = std::move(stroke),
    });
    if (m.onTap) {
      ctx.eventMap().insert(rid, EventHandlers{.stableTargetKey = stableKey, .onTap = m.onTap});
    }
  }

  float const pad = std::max(0.f, m.padding);
  float innerW = std::max(0.f, outerW - 2.f * pad);
  float innerH = std::max(0.f, outerH - 2.f * pad);
  Rect const innerFrame = needEffectLayer ? Rect{pad, pad, innerW, innerH}
                                          : Rect{absOuter.x + pad, absOuter.y + pad, innerW, innerH};

  LayoutConstraints innerCs = scope.outer;
  innerCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  innerCs.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();

  scope.le.setChildFrame(innerFrame);
  ctx.pushConstraints(innerCs);
  impl_->build(ctx);
  ctx.popConstraints();

  if (m.overlay) {
    LayoutConstraints overlayCs = scope.outer;
    overlayCs.maxWidth = outerW > 0.f ? outerW : std::numeric_limits<float>::infinity();
    overlayCs.maxHeight = outerH > 0.f ? outerH : std::numeric_limits<float>::infinity();
    Rect const overFrame = needEffectLayer ? localOuter : absOuter;
    scope.le.setChildFrame(overFrame);
    ctx.pushConstraints(overlayCs);
    m.overlay->build(ctx);
    ctx.popConstraints();
  }
}

Size Element::measureWithModifiersImpl(BuildContext& ctx, LayoutConstraints const& constraints,
                                       LayoutHints const& hints, TextSystem& textSystem) const {
  ElementModifiers const& m = *modifiers_;
  float const pad2 = m.padding * 2.f;
  LayoutConstraints innerCs = constraints;
  if (pad2 > 0.f) {
    if (std::isfinite(innerCs.maxWidth)) {
      innerCs.maxWidth -= pad2;
    }
    if (std::isfinite(innerCs.maxHeight)) {
      innerCs.maxHeight -= pad2;
    }
  }

  Size sz{};
  if (m.overlay) {
    ContainerMeasureScope scope(ctx);
    Size const szUnder = impl_->measure(ctx, innerCs, hints, textSystem);
    Size const szOver = m.overlay->measure(ctx, innerCs, hints, textSystem);
    sz.width = std::max(szUnder.width, szOver.width) + pad2;
    sz.height = std::max(szUnder.height, szOver.height) + pad2;
  } else {
    sz = impl_->measure(ctx, innerCs, hints, textSystem);
    sz.width += pad2;
    sz.height += pad2;
  }
  if (m.frameWidth > 0.f) {
    sz.width = m.frameWidth;
  }
  if (m.frameHeight > 0.f) {
    sz.height = m.frameHeight;
  }
  return sz;
}

Size Element::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                      LayoutHints const& hints, TextSystem& textSystem) const {
  if (envLayer_) {
    EnvironmentStack::current().push(*envLayer_);
  }
  Size sz{};
  MeasureCache* const mc = envLayer_ ? nullptr : ctx.measureCache();
  bool const canMemo =
      mc && impl_->canMemoizeMeasure() && (!modifiers_ || !modifiers_->overlay);

  if (modifiers_ && modifiers_->needsModifierPass()) {
    if (canMemo) {
      MeasureCacheKey const key = makeMeasureCacheKey(measureId_, constraints, hints);
      if (std::optional<Size> const cached = mc->tryGet(key)) {
        ctx.advanceChildSlot();
        sz = *cached;
      } else {
        sz = measureWithModifiersImpl(ctx, constraints, hints, textSystem);
        mc->put(key, sz);
      }
    } else {
      sz = measureWithModifiersImpl(ctx, constraints, hints, textSystem);
    }
  } else if (canMemo) {
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
    , modifiers_(other.modifiers_)
    , measureId_(detail::nextElementMeasureId()) {}

Element& Element::operator=(Element const& other) {
  if (this != &other) {
    impl_ = other.impl_ ? other.impl_->clone() : nullptr;
    flexGrowOverride_ = other.flexGrowOverride_;
    flexShrinkOverride_ = other.flexShrinkOverride_;
    minMainSizeOverride_ = other.minMainSizeOverride_;
    envLayer_ = other.envLayer_;
    modifiers_ = other.modifiers_;
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

Element Element::flex(float grow, float shrink, float minMain) && {
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

  std::shared_ptr<TextLayout> layout;
  if (!value.text.empty()) {
    TextLayoutOptions const opts = textViewLayoutOptions(value, ctx.constraints(), ctx.hints());
    layout = ctx.textSystem().layout(value.text, value.font, value.color, bounds, opts);
  }

  if (layout && !layout->runs.empty()) {
    NodeId const textId = ctx.graph().addText(ctx.parentLayer(), TextNode{
        .layout = layout,
        .origin = {bounds.x, bounds.y},
        .allocation = bounds,
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
  TextLayoutOptions const opts = textViewLayoutOptions(value, c, hints);
  float const mw = std::isfinite(c.maxWidth) ? c.maxWidth : 0.f;
  Size s = ts.measure(value.text, value.font, value.color, mw, opts);
  if (std::isfinite(c.maxWidth) && c.maxWidth > 0.f) {
    s.width = std::min(s.width, c.maxWidth);
  }
  if (std::isfinite(c.maxHeight) && c.maxHeight > 0.f) {
    s.height = std::min(s.height, c.maxHeight);
  }
  return s;
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
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->padding = all;
  return std::move(*this);
}

Element Element::background(FillStyle fill) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->background = std::move(fill);
  return std::move(*this);
}

Element Element::frame(float width, float height) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->frameWidth = width;
  modifiers_->frameHeight = height;
  return std::move(*this);
}

Element Element::border(StrokeStyle stroke) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->border = std::move(stroke);
  return std::move(*this);
}

Element Element::cornerRadius(CornerRadius radius) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->cornerRadius = radius;
  return std::move(*this);
}

Element Element::opacity(float opacity) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->opacity = opacity;
  return std::move(*this);
}

Element Element::offset(Vec2 delta) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->offset = delta;
  return std::move(*this);
}

Element Element::offset(float dx, float dy) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->offset = {dx, dy};
  return std::move(*this);
}

Element Element::clipContent(bool clip) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->clip = clip;
  return std::move(*this);
}

Element Element::overlay(Element over) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->overlay = std::make_unique<Element>(std::move(over));
  return std::move(*this);
}

Element Element::onTapGesture(std::function<void()> handler) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->onTap = std::move(handler);
  return std::move(*this);
}

} // namespace flux
