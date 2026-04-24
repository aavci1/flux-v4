#include <Flux/Core/ComponentKey.hpp>

namespace flux {

std::size_t ComponentKeyHash::operator()(ComponentKey const& k) const noexcept {
  std::size_t seed = k.size();
  LocalIdHash const localHash{};
  for (LocalId const& i : k) {
    std::size_t const h = localHash(i);
    seed ^= h + 0x9e3779b9u + (seed << 6) + (seed >> 2);
  }
  return seed;
}

} // namespace flux
