#pragma once

/// \file Flux/SceneGraph/LayoutConstraints.hpp
///
/// Retained-scene measurement constraints.

#include <limits>

namespace flux {

struct LayoutConstraints {
  float maxWidth = std::numeric_limits<float>::infinity();
  float maxHeight = std::numeric_limits<float>::infinity();
  float minWidth = 0.f;
  float minHeight = 0.f;
};

} // namespace flux
