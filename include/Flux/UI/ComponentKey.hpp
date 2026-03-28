#pragma once

#include <cstddef>
#include <functional>
#include <vector>

namespace flux {

/// Structural position of a composite component in the tree.
/// Encoded as a sequence of child indices from the root.
using ComponentKey = std::vector<std::size_t>;

struct ComponentKeyHash {
  std::size_t operator()(ComponentKey const& k) const noexcept {
    std::size_t seed = k.size();
    for (std::size_t i : k) {
      seed ^= i + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    }
    return seed;
  }
};

} // namespace flux
