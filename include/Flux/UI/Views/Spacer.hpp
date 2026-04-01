#pragma once

/// \file Flux/UI/Views/Spacer.hpp
///
/// Flexible empty region for HStack/VStack: expands along the stack main axis.

#include <Flux/UI/Element.hpp>

namespace flux {

struct Spacer {
  /// Minimum size on the stack main axis before flex distribution.
  float minLength = 0.f;
};

} // namespace flux
