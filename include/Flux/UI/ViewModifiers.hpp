#pragma once

/// \file Flux/UI/ViewModifiers.hpp
///
/// CRTP base providing modifier methods on all view types, removing the need for
/// explicit Element{...} wrapping before calling modifiers.
///
/// Method bodies live at the bottom of Element.hpp (Element must be a complete type).

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>

#include <functional>

namespace flux {

class Element;

/// CRTP base that gives every view struct chained modifier methods.
/// Each method constructs an \ref Element from the derived view and delegates to Element's modifier.
template<typename Derived>
struct ViewModifiers {
  Element padding(float all) &&;
  Element background(FillStyle fill) &&;
  Element frame(float width, float height) &&;
  Element border(StrokeStyle stroke) &&;
  Element cornerRadius(CornerRadius radius) &&;
  Element opacity(float opacity) &&;
  Element offset(Vec2 delta) &&;
  Element offset(float dx, float dy) &&;
  Element clipContent(bool clip) &&;
  Element overlay(Element over) &&;

  Element onTap(std::function<void()> handler) &&;
  Element onPointerDown(std::function<void(Point)> handler) &&;
  Element onPointerUp(std::function<void(Point)> handler) &&;
  Element onPointerMove(std::function<void(Point)> handler) &&;
  Element onScroll(std::function<void(Vec2)> handler) &&;
  Element onKeyDown(std::function<void(KeyCode, Modifiers)> handler) &&;
  Element onKeyUp(std::function<void(KeyCode, Modifiers)> handler) &&;
  Element onTextInput(std::function<void(std::string const&)> handler) &&;
  Element focusable(bool enabled) &&;
  Element cursor(Cursor c) &&;
  Element cursorPassthrough(bool passthrough) &&;

  Element flex(float grow, float shrink = 1.f, float minMain = 0.f) &&;
  template<typename T>
  Element environment(T value) &&;
};

} // namespace flux
