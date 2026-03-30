#pragma once

#include <Flux/UI/ComponentKey.hpp>

#include <algorithm>
#include <cstddef>

namespace flux {

/// True when the first `min(a.size(), b.size())` elements of \p a and \p b match
/// (one key is a prefix of the other along the shared length).
inline bool keySharesPrefix(ComponentKey const& a, ComponentKey const& b) noexcept {
  if (a.empty() || b.empty()) {
    return false;
  }
  std::size_t const len = std::min(a.size(), b.size());
  return std::equal(a.begin(), a.begin() + static_cast<std::ptrdiff_t>(len), b.begin());
}

} // namespace flux
