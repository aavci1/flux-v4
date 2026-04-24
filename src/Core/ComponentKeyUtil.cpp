#include <Flux/Core/ComponentKeyUtil.hpp>

#include <algorithm>

namespace flux {

bool keySharesPrefix(ComponentKey const& a, ComponentKey const& b) noexcept {
  if (a.empty() || b.empty()) {
    return false;
  }
  return a.sharesPrefix(b);
}

} // namespace flux
