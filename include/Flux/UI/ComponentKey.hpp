#pragma once

/// \file Flux/UI/ComponentKey.hpp
///
/// Part of the Flux public API.


#include <cstddef>
#include <vector>

namespace flux {

/// Structural position of a composite component: sequence of child indices
/// from the root, assigned at build time. Stable as long as tree structure
/// is stable (same container types and child counts at each level).
using ComponentKey = std::vector<std::size_t>;

struct ComponentKeyHash {
  std::size_t operator()(ComponentKey const& k) const noexcept;
};

} // namespace flux
