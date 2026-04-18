#pragma once

/// \file Flux/Scene/NodeId.hpp
///
/// Part of the Flux public API.


#include <cstddef>
#include <cstdint>

namespace flux {

struct NodeId {
  constexpr NodeId() = default;
  constexpr explicit NodeId(std::uint64_t raw) : value(raw) {}

  constexpr bool isValid() const noexcept { return value != 0; }
  constexpr bool operator==(NodeId const& other) const noexcept { return value == other.value; }
  constexpr bool operator!=(NodeId const& other) const noexcept { return value != other.value; }

  std::uint64_t value = 0;
};

struct NodeIdHash {
  std::size_t operator()(NodeId const& id) const noexcept { return static_cast<std::size_t>(id.value); }
};

inline constexpr NodeId kInvalidNodeId{};

} // namespace flux
