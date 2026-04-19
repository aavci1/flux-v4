#pragma once

#include <Flux/UI/ViewModifiers.hpp>

namespace flux {

template<typename Derived>
Element ViewModifiers<Derived>::padding(float all) && {
  return Element{std::move(static_cast<Derived&>(*this))}.padding(all);
}

template<typename Derived>
Element ViewModifiers<Derived>::padding(EdgeInsets insets) && {
  return Element{std::move(static_cast<Derived&>(*this))}.padding(insets);
}

template<typename Derived>
Element ViewModifiers<Derived>::padding(float top, float right, float bottom, float left) && {
  return Element{std::move(static_cast<Derived&>(*this))}.padding({top, right, bottom, left});
}

template<typename Derived>
Element ViewModifiers<Derived>::fill(FillStyle style) && {
  return Element{std::move(static_cast<Derived&>(*this))}.fill(std::move(style));
}

template <typename Derived>
Element ViewModifiers<Derived>::fill(Color color) && {
  return Element {std::move(static_cast<Derived &>(*this))}.fill(FillStyle::solid(std::move(color)));
}

template<typename Derived>
Element ViewModifiers<Derived>::shadow(ShadowStyle style) && {
  return Element{std::move(static_cast<Derived&>(*this))}.shadow(std::move(style));
}

template<typename Derived>
Element ViewModifiers<Derived>::size(float width, float height) && {
  return Element{std::move(static_cast<Derived&>(*this))}.size(width, height);
}

template<typename Derived>
Element ViewModifiers<Derived>::width(float w) && {
  return Element{std::move(static_cast<Derived&>(*this))}.width(w);
}

template<typename Derived>
Element ViewModifiers<Derived>::height(float h) && {
  return Element{std::move(static_cast<Derived&>(*this))}.height(h);
}

template<typename Derived>
Element ViewModifiers<Derived>::stroke(StrokeStyle style) && {
  return Element{std::move(static_cast<Derived&>(*this))}.stroke(std::move(style));
}

template <typename Derived>
Element ViewModifiers<Derived>::stroke(Color color, float width) && {
  return Element {std::move(static_cast<Derived &>(*this))}.stroke(StrokeStyle::solid(std::move(color), width));
}

template<typename Derived>
Element ViewModifiers<Derived>::cornerRadius(CornerRadius radius) && {
  return Element{std::move(static_cast<Derived&>(*this))}.cornerRadius(radius);
}

template<typename Derived>
Element ViewModifiers<Derived>::cornerRadius(float radius) && {
  return Element{std::move(static_cast<Derived&>(*this))}.cornerRadius(radius);
}

template<typename Derived>
Element ViewModifiers<Derived>::opacity(float o) && {
  return Element{std::move(static_cast<Derived&>(*this))}.opacity(o);
}

template<typename Derived>
Element ViewModifiers<Derived>::position(Vec2 p) && {
  return Element{std::move(static_cast<Derived&>(*this))}.position(p);
}

template<typename Derived>
Element ViewModifiers<Derived>::position(float x, float y) && {
  return Element{std::move(static_cast<Derived&>(*this))}.position(x, y);
}

template<typename Derived>
Element ViewModifiers<Derived>::translate(Vec2 delta) && {
  return Element{std::move(static_cast<Derived&>(*this))}.translate(delta);
}

template<typename Derived>
Element ViewModifiers<Derived>::translate(float dx, float dy) && {
  return Element{std::move(static_cast<Derived&>(*this))}.translate(dx, dy);
}

template<typename Derived>
Element ViewModifiers<Derived>::clipContent(bool clip) && {
  return Element{std::move(static_cast<Derived&>(*this))}.clipContent(clip);
}

template<typename Derived>
Element ViewModifiers<Derived>::overlay(Element over) && {
  return Element{std::move(static_cast<Derived&>(*this))}.overlay(std::move(over));
}

template<typename Derived>
Element ViewModifiers<Derived>::key(std::string key) && {
  return Element{std::move(static_cast<Derived&>(*this))}.key(std::move(key));
}

template<typename Derived>
Element ViewModifiers<Derived>::onTap(std::function<void()> handler) && {
  return Element{std::move(static_cast<Derived&>(*this))}.onTap(std::move(handler));
}

template<typename Derived>
Element ViewModifiers<Derived>::onPointerDown(std::function<void(Point)> handler) && {
  return Element{std::move(static_cast<Derived&>(*this))}.onPointerDown(std::move(handler));
}

template<typename Derived>
Element ViewModifiers<Derived>::onPointerUp(std::function<void(Point)> handler) && {
  return Element{std::move(static_cast<Derived&>(*this))}.onPointerUp(std::move(handler));
}

template<typename Derived>
Element ViewModifiers<Derived>::onPointerMove(std::function<void(Point)> handler) && {
  return Element{std::move(static_cast<Derived&>(*this))}.onPointerMove(std::move(handler));
}

template<typename Derived>
Element ViewModifiers<Derived>::onScroll(std::function<void(Vec2)> handler) && {
  return Element{std::move(static_cast<Derived&>(*this))}.onScroll(std::move(handler));
}

template<typename Derived>
Element ViewModifiers<Derived>::onKeyDown(std::function<void(KeyCode, Modifiers)> handler) && {
  return Element{std::move(static_cast<Derived&>(*this))}.onKeyDown(std::move(handler));
}

template<typename Derived>
Element ViewModifiers<Derived>::onKeyUp(std::function<void(KeyCode, Modifiers)> handler) && {
  return Element{std::move(static_cast<Derived&>(*this))}.onKeyUp(std::move(handler));
}

template<typename Derived>
Element ViewModifiers<Derived>::onTextInput(std::function<void(std::string const&)> handler) && {
  return Element{std::move(static_cast<Derived&>(*this))}.onTextInput(std::move(handler));
}

template<typename Derived>
Element ViewModifiers<Derived>::focusable(bool enabled) && {
  return Element{std::move(static_cast<Derived&>(*this))}.focusable(enabled);
}

template<typename Derived>
Element ViewModifiers<Derived>::cursor(Cursor c) && {
  return Element{std::move(static_cast<Derived&>(*this))}.cursor(c);
}

template<typename Derived>
Element ViewModifiers<Derived>::flex(float grow, float shrink, float minMain) && {
  return Element{std::move(static_cast<Derived&>(*this))}.flex(grow, shrink, minMain);
}

template<typename Derived>
template<typename T>
Element ViewModifiers<Derived>::environment(T value) && {
  return Element{std::move(static_cast<Derived&>(*this))}.environment(std::move(value));
}

} // namespace flux
