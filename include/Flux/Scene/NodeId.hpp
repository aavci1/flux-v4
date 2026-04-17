#pragma once

/// \file Flux/Scene/NodeId.hpp
///
/// Part of the Flux public API.


#include <cstdint>

namespace flux {

struct NodeId {
  constexpr NodeId() = default;
  constexpr explicit NodeId(std::uint64_t raw) : value(raw) {}
  constexpr NodeId(std::uint32_t slotIndex, std::uint32_t slotGeneration)
      : value((static_cast<std::uint64_t>(slotGeneration) << 32u) | slotIndex) {}

  constexpr bool isValid() const { return value != 0; }
  constexpr bool operator==(NodeId const& other) const { return value == other.value; }

  union {
    std::uint64_t value = 0;
    struct {
      std::uint32_t index;
      std::uint32_t generation;
    };
  };
};

inline constexpr NodeId kInvalidNodeId{};

} // namespace flux
