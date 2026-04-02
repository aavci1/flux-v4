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

  void build(BuildContext&) const;
  Size measure(BuildContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  Path path{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
};

} // namespace flux
