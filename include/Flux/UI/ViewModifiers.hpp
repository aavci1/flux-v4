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
#include <Flux/UI/Environment.hpp>

#include <functional>
#include <string>

namespace flux {

class Element;

/// CRTP base that gives every view struct chained modifier methods.
/// Each method constructs an \ref Element from the derived view and delegates to Element's modifier.
template<typename Derived>
struct ViewModifiers {
  bool operator==(ViewModifiers const&) const noexcept = default;

  Element padding(Reactive::Bindable<float> all) &&;
  Element padding(Reactive::Bindable<EdgeInsets> insets) &&;
  Element padding(Reactive::Bindable<float> top, Reactive::Bindable<float> right,
                  Reactive::Bindable<float> bottom, Reactive::Bindable<float> left) &&;
  Element fill(Reactive::Bindable<FillStyle> style) &&;
  Element fill(Reactive::Bindable<Color> color) &&;
  Element shadow(Reactive::Bindable<ShadowStyle> style) &&;
  Element size(Reactive::Bindable<float> width, Reactive::Bindable<float> height) &&;
  Element width(Reactive::Bindable<float> w) &&;
  Element height(Reactive::Bindable<float> h) &&;
  Element stroke(Reactive::Bindable<StrokeStyle> style) &&;
  Element stroke(Reactive::Bindable<Color> color, Reactive::Bindable<float> width) &&;
  Element cornerRadius(Reactive::Bindable<CornerRadius> radius) &&;
  Element cornerRadius(Reactive::Bindable<float> radius) &&;
  Element opacity(Reactive::Bindable<float> opacity) &&;
  Element position(Reactive::Bindable<Vec2> p) &&;
  Element position(Reactive::Bindable<float> x, Reactive::Bindable<float> y) &&;
  Element translate(Reactive::Bindable<Vec2> delta) &&;
  Element translate(Reactive::Bindable<float> dx, Reactive::Bindable<float> dy) &&;
  Element clipContent(bool clip) &&;
  Element overlay(Element over) &&;
  Element key(std::string key) &&;

  Element onTap(std::function<void()> handler) &&;
  Element onPointerEnter(std::function<void()> handler) &&;
  Element onPointerExit(std::function<void()> handler) &&;
  Element onFocus(std::function<void()> handler) &&;
  Element onBlur(std::function<void()> handler) &&;
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
  template<typename Key>
  Element environment(typename EnvironmentKey<Key>::Value value) &&;

  template<typename Key>
  Element environment(Reactive::Signal<typename EnvironmentKey<Key>::Value> signal) &&;
};

} // namespace flux
