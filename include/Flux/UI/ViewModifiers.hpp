#pragma once

/// \file Flux/UI/ViewModifiers.hpp
///
/// CRTP base providing modifier methods on all view types, removing the need for
/// explicit Element{...} wrapping before calling modifiers.
///
/// Inline bodies live in `Flux/UI/Detail/ViewModifierInlines.hpp` and require `Element` to be complete.

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Reactive/Bindable.hpp>

#include <functional>
#include <string>

namespace flux {

class Element;

/// CRTP base that gives every view struct chained modifier methods.
/// Each method constructs an \ref Element from the derived view and delegates to Element's modifier.
template<typename Derived>
struct ViewModifiers {
  bool operator==(ViewModifiers const&) const noexcept = default;

  Element padding(float all) &&;
  Element padding(EdgeInsets insets) &&;
  Element padding(float top, float right, float bottom, float left) &&;
  Element fill(FillStyle style) &&;
  Element fill(Color color) &&;
  Element fill(Reactive::Bindable<Color> color) &&;
  Element shadow(ShadowStyle style) &&;
  Element size(float width, float height) &&;
  Element size(Reactive::Bindable<float> width, Reactive::Bindable<float> height) &&;
  Element width(float w) &&;
  Element height(float h) &&;
  Element stroke(StrokeStyle style) &&;
  Element stroke(Color color, float width) &&;
  Element cornerRadius(CornerRadius radius) &&;
  Element cornerRadius(Reactive::Bindable<float> radius) &&;
  /// Uniform radius on all corners (same as \c cornerRadius(CornerRadius{all})).
  Element cornerRadius(float radius) &&;
  Element opacity(float opacity) &&;
  Element position(Vec2 p) &&;
  Element position(float x, float y) &&;
  Element position(Reactive::Bindable<float> x, Reactive::Bindable<float> y) &&;
  Element translate(Vec2 delta) &&;
  Element translate(float dx, float dy) &&;
  Element clipContent(bool clip) &&;
  Element overlay(Element over) &&;
  Element key(std::string key) &&;

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

  Element flex(float grow) &&;
  Element flex(float grow, float shrink) &&;
  Element flex(float grow, float shrink, float basis) &&;
  template<typename T>
  Element environment(T value) &&;
};

} // namespace flux
