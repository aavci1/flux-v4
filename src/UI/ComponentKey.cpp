#include <Flux/UI/ComponentKey.hpp>

namespace flux {

std::size_t ComponentKeyHash::operator()(ComponentKey const& k) const noexcept {
  std::size_t seed = k.size();
  for (std::size_t i : k) {
    seed ^= i + 0x9e3779b9u + (seed << 6) + (seed >> 2);
  }
  return seed;
}

} // namespace flux
