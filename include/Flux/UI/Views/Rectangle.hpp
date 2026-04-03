#pragma once

/// \file Flux/UI/Views/Rectangle.hpp
///
/// Filled and/or stroked rectangle primitive.

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/ViewModifiers.hpp>

namespace flux {

/// Axis-aligned rounded rect leaf. Size, layout-space position, flex, corners, and interaction use
/// \ref Element / \ref ViewModifiers (e.g. \c size, \c position, \c translate).
struct Rectangle : ViewModifiers<Rectangle> {
  static constexpr bool memoizable = true;

  void layout(LayoutContext&) const;
  void renderFromLayout(RenderContext&, LayoutNode const&) const;
  Size measure(LayoutContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  // ── Appearance ─────────────────────────────────────────────────────────────

  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
};

} // namespace flux
