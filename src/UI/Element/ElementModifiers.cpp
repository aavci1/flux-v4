#include <Flux/UI/Element.hpp>

namespace flux {

detail::ElementModifiers::ElementModifiers(detail::ElementModifiers const& o)
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

detail::ElementModifiers& detail::ElementModifiers::operator=(detail::ElementModifiers const& o) {
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

detail::ElementModifiers::~ElementModifiers() = default;

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
