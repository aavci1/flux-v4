#include <Flux/UI/Element.hpp>

#include <Flux/UI/Detail/LayoutDebugDump.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/Views/Popover.hpp>

#include <Flux/Core/Cursor.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/RenderContext.hpp>
#include <Flux/UI/MeasureCache.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Detail/LeafBounds.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include <Flux/UI/Views/Rectangle.hpp>

#include "UI/Detail/EventHelpers.hpp"
#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>

namespace flux {

using namespace flux::layout;

ElementModifiers::ElementModifiers(ElementModifiers const& o)
    : padding(o.padding)
    , fill(o.fill)
    , stroke(o.stroke)
    , shadow(o.shadow)
    , cornerRadius(o.cornerRadius)
    , opacity(o.opacity)
    , translation(o.translation)
    , clip(o.clip)
    , positionX(o.positionX)
    , positionY(o.positionY)
    , sizeWidth(o.sizeWidth)
    , sizeHeight(o.sizeHeight)
    , overlay(o.overlay ? std::make_unique<Element>(*o.overlay) : nullptr)
    , onTap(o.onTap)
    , onPointerDown(o.onPointerDown)
    , onPointerUp(o.onPointerUp)
    , onPointerMove(o.onPointerMove)
    , onScroll(o.onScroll)
    , onKeyDown(o.onKeyDown)
    , onKeyUp(o.onKeyUp)
    , onTextInput(o.onTextInput)
    , focusable(o.focusable)
    , cursor(o.cursor) {}

ElementModifiers& ElementModifiers::operator=(ElementModifiers const& o) {
  if (this != &o) {
    padding = o.padding;
    fill = o.fill;
    stroke = o.stroke;
    shadow = o.shadow;
    cornerRadius = o.cornerRadius;
    opacity = o.opacity;
    translation = o.translation;
    clip = o.clip;
    positionX = o.positionX;
    positionY = o.positionY;
    sizeWidth = o.sizeWidth;
    sizeHeight = o.sizeHeight;
    overlay = o.overlay ? std::make_unique<Element>(*o.overlay) : nullptr;
    onTap = o.onTap;
    onPointerDown = o.onPointerDown;
    onPointerUp = o.onPointerUp;
    onPointerMove = o.onPointerMove;
    onScroll = o.onScroll;
    onKeyDown = o.onKeyDown;
    onKeyUp = o.onKeyUp;
    onTextInput = o.onTextInput;
    focusable = o.focusable;
    cursor = o.cursor;
  }
  return *this;
}

ElementModifiers::~ElementModifiers() = default;

namespace {

Rect explicitLeafBox(views::Image const&) {
  return {};
}

std::uint64_t hashCombine(std::uint64_t seed, std::uint64_t value) {
  seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  return seed;
}

std::uint64_t hashFloat(float value) {
  std::uint32_t bits{};
  static_assert(sizeof(bits) == sizeof(value));
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

std::uint64_t modifierMeasureHash(ElementModifiers const& mods) {
  std::uint64_t h = 0x7e4f9a6b31d2c485ull;
  h = hashCombine(h, hashFloat(mods.padding.top));
  h = hashCombine(h, hashFloat(mods.padding.right));
  h = hashCombine(h, hashFloat(mods.padding.bottom));
  h = hashCombine(h, hashFloat(mods.padding.left));
  h = hashCombine(h, hashFloat(mods.sizeWidth));
  h = hashCombine(h, hashFloat(mods.sizeHeight));
  return h;
}

} // namespace

using flux::detail::eventHandlersFromModifiers;
using flux::detail::shouldInsertHandlers;

void Element::layout(LayoutContext& ctx) const {
  Element const* const prevEl = ctx.currentElement();
  ctx.setCurrentElement(this);
  layoutDebugPushElementBuild(measureId_);
  if (envLayer_) {
    EnvironmentStack::current().push(*envLayer_);
  }
  if (modifiers_ && modifiers_->needsModifierPass()) {
    layoutWithModifiers(ctx);
  } else {
    impl_->layout(ctx);
  }
  if (envLayer_) {
    EnvironmentStack::current().pop();
  }
  ctx.setCurrentElement(prevEl);
  layoutDebugPopElementBuild();
}

bool Element::tryRetainedLayout(LayoutContext& ctx) const {
  if (envLayer_ || modifiers_) {
    return false;
  }
  Element const* const prevEl = ctx.currentElement();
  ctx.setCurrentElement(this);
  bool const reused = impl_->tryRetainedLayout(ctx);
  ctx.setCurrentElement(prevEl);
  return reused;
}

void Element::renderFromLayout(RenderContext& ctx, LayoutNode const& node) const {
  impl_->renderFromLayout(ctx, node);
}

void Element::layoutWithModifiers(LayoutContext& ctx) const {
  ElementModifiers const& m = *modifiers_;
  ComponentKey const stableKey = ctx.leafComponentKey();
  ContainerLayoutScope scope(ctx);
  scope.parentFrame.x += m.positionX;
  scope.parentFrame.y += m.positionY;
  float const assignedW = assignedSpan(scope.parentFrame.width, scope.outer.maxWidth);
  float const assignedH = assignedSpan(scope.parentFrame.height, scope.outer.maxHeight);
  float outerW = std::max(0.f, assignedW);
  float outerH = std::max(0.f, assignedH);

  bool const needEffectLayer = m.opacity < 1.f - 1e-6f || std::fabs(m.translation.x) > 1e-6f ||
                               std::fabs(m.translation.y) > 1e-6f || m.clip;

  if (needEffectLayer) {
    Mat3 const L =
        Mat3::translate(scope.parentFrame.x + m.translation.x, scope.parentFrame.y + m.translation.y);
    ctx.pushLayerWorldTransform(L);
  }

  Rect const absOuter = scope.parentFrame;
  Rect const localOuter{0.f, 0.f, outerW, outerH};
  Rect const bgBounds = needEffectLayer ? localOuter : absOuter;

  LayoutNode mod{};
  mod.kind = LayoutNode::Kind::Modifier;
  mod.modifiers = &m;
  mod.frame = bgBounds;
  mod.componentKey = stableKey;
  mod.element = ctx.currentElement();
  mod.constraints = scope.outer;
  mod.hints = ctx.hints();
  if (needEffectLayer) {
    mod.modifierHasEffectLayer = true;
    mod.modifierLayerTransform =
        Mat3::translate(scope.parentFrame.x + m.translation.x, scope.parentFrame.y + m.translation.y);
  }
  LayoutNodeId const modId = ctx.pushLayoutNode(std::move(mod));
  ctx.registerCompositeSubtreeRootIfPending(modId);
  ctx.pushLayoutParent(modId);

  float const padL = std::max(0.f, m.padding.left);
  float const padR = std::max(0.f, m.padding.right);
  float const padT = std::max(0.f, m.padding.top);
  float const padB = std::max(0.f, m.padding.bottom);
  float const padW = padL + padR;
  float const padH = padT + padB;
  float innerW = std::max(0.f, outerW - padW);
  float innerH = std::max(0.f, outerH - padH);
  Rect const innerFrame = needEffectLayer ? Rect{padL, padT, innerW, innerH}
                                          : Rect{absOuter.x + padL, absOuter.y + padT, innerW, innerH};

  LayoutConstraints innerCs = scope.outer;
  innerCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  innerCs.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();
  if (padW > 0.f || padH > 0.f) {
    innerCs.minWidth = std::max(0.f, innerCs.minWidth - padW);
    innerCs.minHeight = std::max(0.f, innerCs.minHeight - padH);
  }
  if (std::isfinite(innerCs.maxWidth)) {
    innerCs.minWidth = std::min(innerCs.minWidth, innerCs.maxWidth);
  }
  if (std::isfinite(innerCs.maxHeight)) {
    innerCs.minHeight = std::min(innerCs.minHeight, innerCs.maxHeight);
  }

  LayoutHints const preservedHints = ctx.hints();

  scope.le.setChildFrame(innerFrame);
  ctx.pushConstraints(innerCs, preservedHints);
  ctx.pushActiveElementModifiers(&m);
  if (StateStore* const store = StateStore::current()) {
    store->pushCompositeElementModifiers(&m);
  }
  impl_->layout(ctx);
  if (StateStore* const store = StateStore::current()) {
    store->popCompositeElementModifiers();
  }
  ctx.popActiveElementModifiers();
  ctx.popConstraints();

  if (m.overlay) {
    LayoutConstraints overlayCs = scope.outer;
    overlayCs.maxWidth = outerW > 0.f ? outerW : std::numeric_limits<float>::infinity();
    overlayCs.maxHeight = outerH > 0.f ? outerH : std::numeric_limits<float>::infinity();
    Rect const overFrame = needEffectLayer ? localOuter : absOuter;
    scope.le.setChildFrame(overFrame);
    ctx.pushConstraints(overlayCs, ctx.hints());
    m.overlay->layout(ctx);
    ctx.popConstraints();
  }

  ctx.popLayoutParent();
  if (needEffectLayer) {
    ctx.popLayerWorldTransform();
  }
}

Size Element::measureWithModifiersImpl(LayoutContext& ctx, LayoutConstraints const& constraints,
                                       LayoutHints const& hints, TextSystem& textSystem) const {
  ElementModifiers const& m = *modifiers_;
  float const padL = std::max(0.f, m.padding.left);
  float const padR = std::max(0.f, m.padding.right);
  float const padT = std::max(0.f, m.padding.top);
  float const padB = std::max(0.f, m.padding.bottom);
  float const padW = padL + padR;
  float const padH = padT + padB;
  LayoutConstraints innerCs = constraints;
  if (padW > 0.f || padH > 0.f) {
    if (std::isfinite(innerCs.maxWidth)) {
      innerCs.maxWidth -= padW;
    }
    if (std::isfinite(innerCs.maxHeight)) {
      innerCs.maxHeight -= padH;
    }
    innerCs.minWidth = std::max(0.f, innerCs.minWidth - padW);
    innerCs.minHeight = std::max(0.f, innerCs.minHeight - padH);
  }
  if (std::isfinite(innerCs.maxWidth)) {
    innerCs.minWidth = std::min(innerCs.minWidth, innerCs.maxWidth);
  }
  if (std::isfinite(innerCs.maxHeight)) {
    innerCs.minHeight = std::min(innerCs.minHeight, innerCs.maxHeight);
  }

  Size sz{};
  if (m.overlay) {
    ContainerMeasureScope scope(ctx);
    if (StateStore* const store = StateStore::current()) {
      store->pushCompositeElementModifiers(&m);
    }
    Size const szUnder = impl_->measure(ctx, innerCs, hints, textSystem);
    if (StateStore* const store = StateStore::current()) {
      store->popCompositeElementModifiers();
    }
    Size const szOver = m.overlay->measure(ctx, innerCs, hints, textSystem);
    sz.width = std::max(szUnder.width, szOver.width) + padW;
    sz.height = std::max(szUnder.height, szOver.height) + padH;
  } else {
    // Mirror layoutWithModifiers(): consume any pending composite subtree skip and
    // scope child indexing for the modifier shell even without an overlay subtree.
    ContainerMeasureScope scope(ctx);
    if (StateStore* const store = StateStore::current()) {
      store->pushCompositeElementModifiers(&m);
    }
    sz = impl_->measure(ctx, innerCs, hints, textSystem);
    if (StateStore* const store = StateStore::current()) {
      store->popCompositeElementModifiers();
    }
    sz.width += padW;
    sz.height += padH;
  }
  if (m.sizeWidth > 0.f) {
    sz.width = m.sizeWidth;
  }
  if (m.sizeHeight > 0.f) {
    sz.height = m.sizeHeight;
  }
  return sz;
}

Size Element::measure(LayoutContext& ctx, LayoutConstraints const& constraints,
                      LayoutHints const& hints, TextSystem& textSystem) const {
  if (envLayer_) {
    EnvironmentStack::current().push(*envLayer_);
  }
  Size sz{};
  MeasureCache* const mc = envLayer_ ? nullptr : ctx.measureCache();
  bool const canMemo =
      mc && impl_->canMemoizeMeasure() && (!modifiers_ || !modifiers_->overlay);
  std::uint64_t const measureIdentity = [&] {
    std::uint64_t h = impl_->measureCacheToken().value_or(measureId_);
    if (modifiers_) {
      h = hashCombine(h, modifierMeasureHash(*modifiers_));
    }
    if (minMainSizeOverride_) {
      h = hashCombine(h, hashFloat(*minMainSizeOverride_));
    }
    return h;
  }();

  if (modifiers_ && modifiers_->needsModifierPass()) {
    if (canMemo) {
      MeasureCacheKey const key = makeMeasureCacheKey(measureIdentity, constraints, hints);
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
    MeasureCacheKey const key = makeMeasureCacheKey(measureIdentity, constraints, hints);
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

void Rectangle::layout(LayoutContext& ctx) const {
  ComponentKey const stableKey = ctx.leafComponentKey();
  ctx.advanceChildSlot();
  Rect explicitFromMods{};
  if (ElementModifiers const* mods = ctx.activeElementModifiers()) {
    if (mods->sizeWidth > 0.f) {
      explicitFromMods.width = mods->sizeWidth;
    }
    if (mods->sizeHeight > 0.f) {
      explicitFromMods.height = mods->sizeHeight;
    }
  }
  Rect const bounds = flux::detail::resolveLeafLayoutBounds(
      explicitFromMods, ctx.layoutEngine().consumeAssignedFrame(), ctx.constraints(), ctx.hints());
  LayoutNode n{};
  n.kind = LayoutNode::Kind::Leaf;
  n.frame = bounds;
  n.componentKey = stableKey;
  n.element = ctx.currentElement();
  n.constraints = ctx.constraints();
  n.hints = ctx.hints();
  LayoutNodeId const id = ctx.pushLayoutNode(std::move(n));
  ctx.registerCompositeSubtreeRootIfPending(id);
  layoutDebugLogLeaf("Rectangle", ctx.constraints(), bounds, detail::flexGrowOf(*this),
                     detail::flexShrinkOf(*this), detail::minMainSizeOf(*this));
}

void Rectangle::renderFromLayout(RenderContext& ctx, LayoutNode const& node) const {
  ComponentKey const stableKey = node.componentKey;
  Rect const bounds = node.frame;
  CornerRadius cornerR{};
  FillStyle fillEff = FillStyle::none();
  StrokeStyle strokeEff = StrokeStyle::none();
  ShadowStyle shadowEff = ShadowStyle::none();
  if (ElementModifiers const* mods = ctx.activeElementModifiers()) {
    cornerR = mods->cornerRadius;
    fillEff = mods->fill;
    strokeEff = mods->stroke;
    shadowEff = mods->shadow;
  }
  NodeId const id = ctx.graph().addRect(ctx.parentLayer(), RectNode{
      .bounds = bounds,
      .cornerRadius = cornerR,
      .fill = fillEff,
      .stroke = strokeEff,
      .shadow = shadowEff,
  });
  if (ElementModifiers const* mods = ctx.activeElementModifiers()) {
    if (!ctx.suppressLeafModifierEvents()) {
      EventHandlers h = eventHandlersFromModifiers(*mods, stableKey, bounds);
      if (shouldInsertHandlers(h)) {
        ctx.eventMap().insert(id, std::move(h));
      }
    }
  }
}

Size Rectangle::measure(LayoutContext& ctx, LayoutConstraints const& c, LayoutHints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  float const w = std::isfinite(c.maxWidth) ? c.maxWidth : 0.f;
  return {w, 0.f};
}

void views::Image::layout(LayoutContext& ctx) const {
  ComponentKey const stableKey = ctx.leafComponentKey();
  ctx.advanceChildSlot();
  if (!source) {
    return;
  }
  Rect const bounds = flux::detail::resolveLeafLayoutBounds(
      explicitLeafBox(*this), ctx.layoutEngine().consumeAssignedFrame(), ctx.constraints(), ctx.hints());
  LayoutNode n{};
  n.kind = LayoutNode::Kind::Leaf;
  n.frame = bounds;
  n.componentKey = stableKey;
  n.element = ctx.currentElement();
  n.constraints = ctx.constraints();
  n.hints = ctx.hints();
  LayoutNodeId const id = ctx.pushLayoutNode(std::move(n));
  ctx.registerCompositeSubtreeRootIfPending(id);
  layoutDebugLogLeaf("Image", ctx.constraints(), bounds, detail::flexGrowOf(*this),
                     detail::flexShrinkOf(*this), detail::minMainSizeOf(*this));
}

void views::Image::renderFromLayout(RenderContext& ctx, LayoutNode const& node) const {
  if (!source) {
    return;
  }
  ComponentKey const stableKey = node.componentKey;
  Rect const bounds = node.frame;
  NodeId const id = ctx.graph().addImage(ctx.parentLayer(), ImageNode{
      .image = source,
      .bounds = bounds,
      .fillMode = fillMode,
  });
  if (ElementModifiers const* mods = ctx.activeElementModifiers()) {
    if (!ctx.suppressLeafModifierEvents()) {
      EventHandlers h = eventHandlersFromModifiers(*mods, stableKey, bounds);
      if (shouldInsertHandlers(h)) {
        ctx.eventMap().insert(id, std::move(h));
      }
    }
  }
}

Size views::Image::measure(LayoutContext& ctx, LayoutConstraints const& c, LayoutHints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  float const w = std::isfinite(c.maxWidth) ? c.maxWidth : 0.f;
  float const h = std::isfinite(c.maxHeight) ? c.maxHeight : 0.f;
  return {w, h};
}

void PathShape::layout(LayoutContext& ctx) const {
  ComponentKey const stableKey = ctx.leafComponentKey();
  ctx.advanceChildSlot();
  Rect const cf = ctx.layoutEngine().consumeAssignedFrame();
  LayoutNode n{};
  n.kind = LayoutNode::Kind::Leaf;
  n.frame = cf;
  n.componentKey = stableKey;
  n.element = ctx.currentElement();
  n.constraints = ctx.constraints();
  n.hints = ctx.hints();
  LayoutNodeId const id = ctx.pushLayoutNode(std::move(n));
  ctx.registerCompositeSubtreeRootIfPending(id);
  layoutDebugLogLeaf("PathShape", ctx.constraints(), cf, detail::flexGrowOf(*this),
                     detail::flexShrinkOf(*this), detail::minMainSizeOf(*this));
}

void PathShape::renderFromLayout(RenderContext& ctx, LayoutNode const& node) const {
  FillStyle fillEff = FillStyle::none();
  StrokeStyle strokeEff = StrokeStyle::none();
  ShadowStyle shadowEff = ShadowStyle::none();
  if (ElementModifiers const* mods = ctx.activeElementModifiers()) {
    fillEff = mods->fill;
    strokeEff = mods->stroke;
    shadowEff = mods->shadow;
  }
  PathNode pathNode{.path = path, .fill = fillEff, .stroke = strokeEff, .shadow = shadowEff};
  Rect const cf = node.frame;
  Rect const pb = path.getBounds();
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
    ctx.graph().addPath(lid, std::move(pathNode));
  } else {
    ctx.graph().addPath(ctx.parentLayer(), std::move(pathNode));
  }
}

Size PathShape::measure(LayoutContext& ctx, LayoutConstraints const&, LayoutHints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  Rect const b = path.getBounds();
  return {b.width, b.height};
}

void Line::layout(LayoutContext& ctx) const {
  ComponentKey const stableKey = ctx.leafComponentKey();
  ctx.advanceChildSlot();
  float const minX = std::min(from.x, to.x);
  float const maxX = std::max(from.x, to.x);
  float const minY = std::min(from.y, to.y);
  float const maxY = std::max(from.y, to.y);
  Rect const lineBounds{minX, minY, maxX - minX, maxY - minY};
  LayoutNode n{};
  n.kind = LayoutNode::Kind::Leaf;
  n.frame = lineBounds;
  n.componentKey = stableKey;
  n.element = ctx.currentElement();
  n.constraints = ctx.constraints();
  n.hints = ctx.hints();
  LayoutNodeId const id = ctx.pushLayoutNode(std::move(n));
  ctx.registerCompositeSubtreeRootIfPending(id);
  layoutDebugLogLeaf("Line", ctx.constraints(), lineBounds, detail::flexGrowOf(*this),
                     detail::flexShrinkOf(*this), detail::minMainSizeOf(*this));
}

void Line::renderFromLayout(RenderContext& ctx, LayoutNode const&) const {
  ctx.graph().addLine(ctx.parentLayer(), LineNode{
      .from = from,
      .to = to,
      .stroke = stroke,
  });
}

std::uint64_t Line::measureCacheKey() const noexcept {
  std::uint64_t h = 0x36a7bde918c24f05ull;
  h = hashCombine(h, hashFloat(from.x));
  h = hashCombine(h, hashFloat(from.y));
  h = hashCombine(h, hashFloat(to.x));
  h = hashCombine(h, hashFloat(to.y));
  return h;
}

Size Line::measure(LayoutContext& ctx, LayoutConstraints const&, LayoutHints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  float const minX = std::min(from.x, to.x);
  float const maxX = std::max(from.x, to.x);
  float const minY = std::min(from.y, to.y);
  float const maxY = std::max(from.y, to.y);
  return {maxX - minX, maxY - minY};
}

Element Element::padding(float all) && {
  return std::move(*this).padding(EdgeInsets::uniform(all));
}

Element Element::padding(EdgeInsets insets) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->padding = std::move(insets);
  return std::move(*this);
}

Element Element::padding(float top, float right, float bottom, float left) && {
  return std::move(*this).padding(EdgeInsets{.top = top, .right = right, .bottom = bottom, .left = left});
}

Element Element::fill(FillStyle style) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->fill = std::move(style);
  return std::move(*this);
}

Element Element::shadow(ShadowStyle style) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->shadow = std::move(style);
  return std::move(*this);
}

Element Element::size(float width, float height) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->sizeWidth = width;
  modifiers_->sizeHeight = height;
  return std::move(*this);
}

Element Element::width(float w) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->sizeWidth = w;
  return std::move(*this);
}

Element Element::height(float h) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->sizeHeight = h;
  return std::move(*this);
}

Element Element::stroke(StrokeStyle style) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->stroke = std::move(style);
  return std::move(*this);
}

Element Element::cornerRadius(CornerRadius radius) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->cornerRadius = radius;
  return std::move(*this);
}

Element Element::cornerRadius(float radius) && {
  return std::move(*this).cornerRadius(CornerRadius(radius));
}

Element Element::opacity(float opacity) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->opacity = opacity;
  return std::move(*this);
}

Element Element::position(Vec2 p) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->positionX = p.x;
  modifiers_->positionY = p.y;
  return std::move(*this);
}

Element Element::position(float x, float y) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->positionX = x;
  modifiers_->positionY = y;
  return std::move(*this);
}

Element Element::translate(Vec2 delta) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->translation = delta;
  return std::move(*this);
}

Element Element::translate(float dx, float dy) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->translation = {dx, dy};
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

Element Element::onTap(std::function<void()> handler) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->onTap = std::move(handler);
  return std::move(*this);
}

Element Element::onPointerDown(std::function<void(Point)> handler) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->onPointerDown = std::move(handler);
  return std::move(*this);
}

Element Element::onPointerUp(std::function<void(Point)> handler) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->onPointerUp = std::move(handler);
  return std::move(*this);
}

Element Element::onPointerMove(std::function<void(Point)> handler) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->onPointerMove = std::move(handler);
  return std::move(*this);
}

Element Element::onScroll(std::function<void(Vec2)> handler) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->onScroll = std::move(handler);
  return std::move(*this);
}

Element Element::onKeyDown(std::function<void(KeyCode, Modifiers)> handler) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->onKeyDown = std::move(handler);
  return std::move(*this);
}

Element Element::onKeyUp(std::function<void(KeyCode, Modifiers)> handler) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->onKeyUp = std::move(handler);
  return std::move(*this);
}

Element Element::onTextInput(std::function<void(std::string const&)> handler) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->onTextInput = std::move(handler);
  return std::move(*this);
}

Element Element::focusable(bool enabled) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->focusable = enabled;
  return std::move(*this);
}

Element Element::cursor(Cursor c) && {
  if (!modifiers_) {
    modifiers_.emplace();
  }
  modifiers_->cursor = c;
  return std::move(*this);
}

} // namespace flux
