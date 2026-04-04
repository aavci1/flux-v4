#pragma once

/// \file Flux/UI/Views/Rectangle.hpp
///
/// Axis-aligned rounded rectangle primitive; fill, stroke, and shadow come from \ref Element modifiers
/// (\c fill, \c stroke, \c shadow).

#include <Flux/Core/Types.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/ViewModifiers.hpp>

namespace flux {

/// Axis-aligned rounded rect leaf. Size, layout-space position, flex, corners, paint, and interaction use
/// \ref Element / \ref ViewModifiers (e.g. \c size, \c position, \c fill, \c stroke).
struct Rectangle : ViewModifiers<Rectangle> {
  static constexpr bool memoizable = true;

  void layout(LayoutContext&) const;
  void renderFromLayout(RenderContext&, LayoutNode const&) const;
  Size measure(LayoutContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;
};

} // namespace flux
