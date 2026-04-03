#pragma once

/// \file Flux/UI/Views/PathShape.hpp
///
/// Part of the Flux public API.


#include <Flux/Graphics/Path.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/ViewModifiers.hpp>

namespace flux {

/// Scene path primitive (name avoids clashing with `flux::Path`).
struct PathShape : ViewModifiers<PathShape> {
  static constexpr bool memoizable = true;

  void layout(LayoutContext&) const;
  void renderFromLayout(RenderContext&, LayoutNode const&) const;
  Size measure(LayoutContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  Path path{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
  ShadowStyle shadow = ShadowStyle::none();
};

} // namespace flux
