#pragma once

/// \file Flux/Scene/NodeId.hpp
///
/// Part of the Flux public API.


#include <cstdint>

namespace flux {

struct NodeId {
  std::uint32_t index = 0;
  std::uint32_t generation = 0;

  constexpr bool isValid() const { return generation != 0; }
  constexpr bool operator==(NodeId const&) const = default;
};

inline constexpr NodeId kInvalidNodeId{};

} // namespace flux
