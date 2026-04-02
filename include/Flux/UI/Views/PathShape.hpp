#pragma once

/// \file Flux/UI/Views/PathShape.hpp
///
/// Part of the Flux public API.


#include <Flux/Graphics/Path.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/ViewModifiers.hpp>

namespace flux {

/// Scene path primitive (name avoids clashing with `flux::Path`).
struct PathShape : ViewModifiers<PathShape> {
  Path path{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
};

} // namespace flux
