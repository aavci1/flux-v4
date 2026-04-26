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
  writableModifiers().padding = std::move(insets);
  return std::move(*this);
}

Element Element::padding(float top, float right, float bottom, float left) && {
  return std::move(*this).padding(EdgeInsets{.top = top, .right = right, .bottom = bottom, .left = left});
}

Element Element::fill(FillStyle style) && {
  writableModifiers().fill = std::move(style);
  return std::move(*this);
}

Element Element::fill(Color color) && {
  writableModifiers().fill = FillStyle::solid(std::move(color));
  return std::move(*this);
}

Element Element::shadow(ShadowStyle style) && {
  writableModifiers().shadow = std::move(style);
  return std::move(*this);
}

Element Element::size(float width, float height) && {
  detail::ElementModifiers& modifiers = writableModifiers();
  modifiers.sizeWidth = width;
  modifiers.sizeHeight = height;
  return std::move(*this);
}

Element Element::width(float w) && {
  writableModifiers().sizeWidth = w;
  return std::move(*this);
}

Element Element::height(float h) && {
  writableModifiers().sizeHeight = h;
  return std::move(*this);
}

Element Element::stroke(StrokeStyle style) && {
  writableModifiers().stroke = std::move(style);
  return std::move(*this);
}

Element Element::stroke(Color color, float width) && {
  writableModifiers().stroke = StrokeStyle::solid(std::move(color), width);
  return std::move(*this);
}

Element Element::cornerRadius(CornerRadius radius) && {
  writableModifiers().cornerRadius = radius;
  return std::move(*this);
}

Element Element::cornerRadius(float radius) && {
  return std::move(*this).cornerRadius(CornerRadius(radius));
}

Element Element::opacity(float opacity) && {
  writableModifiers().opacity = opacity;
  return std::move(*this);
}

Element Element::position(Vec2 p) && {
  detail::ElementModifiers& modifiers = writableModifiers();
  modifiers.positionX = p.x;
  modifiers.positionY = p.y;
  return std::move(*this);
}

Element Element::position(float x, float y) && {
  detail::ElementModifiers& modifiers = writableModifiers();
  modifiers.positionX = x;
  modifiers.positionY = y;
  return std::move(*this);
}

Element Element::translate(Vec2 delta) && {
  writableModifiers().translation = delta;
  return std::move(*this);
}

Element Element::translate(float dx, float dy) && {
  writableModifiers().translation = {dx, dy};
  return std::move(*this);
}

Element Element::clipContent(bool clip) && {
  writableModifiers().clip = clip;
  return std::move(*this);
}

Element Element::overlay(Element over) && {
  writableModifiers().overlay = std::make_unique<Element>(std::move(over));
  return std::move(*this);
}

Element Element::onTap(std::function<void()> handler) && {
  writableModifiers().onTap = std::move(handler);
  return std::move(*this);
}

Element Element::onPointerDown(std::function<void(Point)> handler) && {
  writableModifiers().onPointerDown = std::move(handler);
  return std::move(*this);
}

Element Element::onPointerUp(std::function<void(Point)> handler) && {
  writableModifiers().onPointerUp = std::move(handler);
  return std::move(*this);
}

Element Element::onPointerMove(std::function<void(Point)> handler) && {
  writableModifiers().onPointerMove = std::move(handler);
  return std::move(*this);
}

Element Element::onScroll(std::function<void(Vec2)> handler) && {
  writableModifiers().onScroll = std::move(handler);
  return std::move(*this);
}

Element Element::onKeyDown(std::function<void(KeyCode, Modifiers)> handler) && {
  writableModifiers().onKeyDown = std::move(handler);
  return std::move(*this);
}

Element Element::onKeyUp(std::function<void(KeyCode, Modifiers)> handler) && {
  writableModifiers().onKeyUp = std::move(handler);
  return std::move(*this);
}

Element Element::onTextInput(std::function<void(std::string const&)> handler) && {
  writableModifiers().onTextInput = std::move(handler);
  return std::move(*this);
}

Element Element::focusable(bool enabled) && {
  writableModifiers().focusable = enabled;
  return std::move(*this);
}

Element Element::cursor(Cursor c) && {
  writableModifiers().cursor = c;
  return std::move(*this);
}

} // namespace flux
