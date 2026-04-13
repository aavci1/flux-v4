#pragma once

/// \file Flux/UI/Invalidation.hpp
///
/// Internal invalidation types for retained / incremental UI updates.

#include <Flux/UI/ComponentKey.hpp>

#include <cstdint>

namespace flux {

enum class InvalidationKind : std::uint8_t {
  Composite = 0,
  Transform = 1,
  Paint = 2,
  Layout = 3,
  Build = 4,
};

constexpr bool invalidationKindAtLeast(InvalidationKind lhs, InvalidationKind rhs) noexcept {
  return static_cast<std::uint8_t>(lhs) >= static_cast<std::uint8_t>(rhs);
}

struct InvalidationRequest {
  ComponentKey key{};
  InvalidationKind kind = InvalidationKind::Build;
};

} // namespace flux
