#pragma once

#include <Flux/Graphics/Path.hpp>
#include <Flux/Graphics/Styles.hpp>

namespace flux {

/// Scene path primitive (name avoids clashing with `flux::Path`).
struct PathShape {
  Path path{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
};

} // namespace flux
