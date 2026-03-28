#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>

namespace flux {

struct Line {
  Point from{};
  Point to{};
  StrokeStyle stroke{};
};

} // namespace flux
