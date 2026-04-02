#pragma once

/// \file Flux/UI/ViewModifiers.hpp
///
/// CRTP base providing modifier methods on all view types, removing the need for
/// explicit Element{...} wrapping before calling modifiers.
///
/// Method names use a \c with* prefix to avoid collisions with same-named data members
/// on view structs (e.g. \c Text::padding vs \c ViewModifiers::withPadding).
/// After the first call returns \ref Element, subsequent chained calls use Element's
/// original method names (\c .padding(), \c .background(), etc.).
///
/// Method bodies live at the bottom of Element.hpp (Element must be a complete type).

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>

#include <functional>

namespace flux {

class Element;

/// CRTP base that gives every view struct chained modifier methods.
/// Each method constructs an \ref Element from the derived view and delegates to Element's modifier.
template<typename Derived>
struct ViewModifiers {
  Element withPadding(float all) &&;
  Element withBackground(FillStyle fill) &&;
  Element withFrame(float width, float height) &&;
  Element withBorder(StrokeStyle stroke) &&;
  Element withCornerRadius(CornerRadius radius) &&;
  Element withOpacity(float opacity) &&;
  Element withOffset(Vec2 delta) &&;
  Element withOffset(float dx, float dy) &&;
  Element withClip(bool clip) &&;
  Element withOverlay(Element over) &&;
  Element withTapGesture(std::function<void()> handler) &&;
  Element withFlex(float grow, float shrink = 1.f, float minMain = 0.f) &&;
  template<typename T>
  Element withEnvironment(T value) &&;
};

} // namespace flux
