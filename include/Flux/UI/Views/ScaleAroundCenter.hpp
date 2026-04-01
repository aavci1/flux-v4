#pragma once

/// \file Flux/UI/Views/ScaleAroundCenter.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/Element.hpp>

namespace flux {

/// Scales a single child around the center of the layout slot (used for press feedback).
struct ScaleAroundCenter {
  float scale = 1.f;
  float flexGrow = 0.f;
  float flexShrink = 0.f;
  float minSize = 0.f;
  Element child;
};

} // namespace flux
