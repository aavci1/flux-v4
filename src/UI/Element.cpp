#include <Flux/UI/Element.hpp>

#include <Flux/UI/Detail/LayoutDebugDump.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/Views/Popover.hpp>

#include <Flux/Core/Cursor.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/Graphics/TextSystem.hpp>

#include <Flux/UI/Views/Rectangle.hpp>

#include "UI/Layout/ContainerScope.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace flux {

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

struct ChildLocalIdScope {
  MeasureContext& ctx;

  ChildLocalIdScope(MeasureContext& context, std::optional<std::string> const& explicitKey)
      : ctx(context) {
    if (explicitKey.has_value()) {
      ctx.pushExplicitChildLocalId(LocalId::fromString(*explicitKey));
    } else {
      ctx.pushExplicitChildLocalId(std::nullopt);
    }
  }

  ~ChildLocalIdScope() { ctx.popExplicitChildLocalId(); }
};

} // namespace

Size Element::measureWithModifiersImpl(MeasureContext& ctx, LayoutConstraints const& constraints,
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

Size Element::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                      LayoutHints const& hints, TextSystem& textSystem) const {
  ChildLocalIdScope const childIdScope{ctx, key_};
  Element const* const prevEl = ctx.currentElement();
  ctx.setCurrentElement(this);
  if (envLayer_) {
    EnvironmentStack::current().push(*envLayer_);
  }
  Size const sz = modifiers_ && modifiers_->needsModifierPass() ? measureWithModifiersImpl(ctx, constraints, hints, textSystem)
                                                                : impl_->measure(ctx, constraints, hints, textSystem);
  if (envLayer_) {
    EnvironmentStack::current().pop();
  }
  ctx.setCurrentElement(prevEl);
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
    , key_(other.key_)
    , measureId_(detail::nextElementMeasureId()) {}

Element& Element::operator=(Element const& other) {
  if (this != &other) {
    impl_ = other.impl_ ? other.impl_->clone() : nullptr;
    flexGrowOverride_ = other.flexGrowOverride_;
    flexShrinkOverride_ = other.flexShrinkOverride_;
    minMainSizeOverride_ = other.minMainSizeOverride_;
    envLayer_ = other.envLayer_;
    modifiers_ = other.modifiers_;
    key_ = other.key_;
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

Element Element::key(std::string key) && {
  key_ = std::move(key);
  return std::move(*this);
}

Size Rectangle::measure(MeasureContext& ctx, LayoutConstraints const& c, LayoutHints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  float const w = std::isfinite(c.maxWidth) ? c.maxWidth : 0.f;
  return {w, 0.f};
}

Size views::Image::measure(MeasureContext& ctx, LayoutConstraints const& c, LayoutHints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  float const w = std::isfinite(c.maxWidth) ? c.maxWidth : 0.f;
  float const h = std::isfinite(c.maxHeight) ? c.maxHeight : 0.f;
  return {w, h};
}

Size PathShape::measure(MeasureContext& ctx, LayoutConstraints const&, LayoutHints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  Rect const b = path.getBounds();
  return {b.width, b.height};
}

Size Line::measure(MeasureContext& ctx, LayoutConstraints const&, LayoutHints const&, TextSystem&) const {
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
