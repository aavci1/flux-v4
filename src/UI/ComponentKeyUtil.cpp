#include <Flux/UI/ComponentKeyUtil.hpp>

#include <algorithm>

namespace flux {

bool keySharesPrefix(ComponentKey const& a, ComponentKey const& b) noexcept {
  if (a.empty() || b.empty()) {
    return false;
  }
  std::size_t const len = std::min(a.size(), b.size());
  return std::equal(a.begin(), a.begin() + static_cast<std::ptrdiff_t>(len), b.begin());
}

} // namespace flux
