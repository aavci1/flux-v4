#pragma once

/// \file Flux/Scene/LocalId.hpp
///
/// Part of the Flux public API.

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace flux {

/// Per-parent child identity used for keyed reconciliation and component paths.
///
/// Positional children use `fromIndex(i)`. Explicit keys hash the user string into `value`
/// and set `kind == Keyed`, so keyed and positional ids never compare equal.
struct LocalId {
  enum class Kind : std::uint8_t {
    Positional,
    Keyed,
  };

  constexpr LocalId() = default;

  /// Positional convenience so existing `push_back(i)` style code keeps compiling.
  constexpr LocalId(std::size_t index)
      : kind(Kind::Positional)
      , value(static_cast<std::uint64_t>(index) + 1ull) {}

  static constexpr LocalId fromIndex(std::size_t index) { return LocalId{index}; }

  static LocalId fromString(std::string_view key) {
    LocalId id;
    id.kind = Kind::Keyed;
    id.value = hashKeyString(key);
    return id;
  }

  constexpr bool operator==(LocalId const&) const = default;

  Kind kind = Kind::Positional;
  std::uint64_t value = 0;

private:
  static std::uint64_t hashKeyString(std::string_view key) {
    // 64-bit FNV-1a. Stable across platforms and good enough for UI identity hashing.
    std::uint64_t h = 14695981039346656037ull;
    for (unsigned char ch : key) {
      h ^= static_cast<std::uint64_t>(ch);
      h *= 1099511628211ull;
    }
    return h == 0 ? 1ull : h;
  }
};

struct LocalIdHash {
  std::size_t operator()(LocalId const& id) const noexcept {
    std::size_t seed = static_cast<std::size_t>(id.value);
    seed ^= static_cast<std::size_t>(id.kind) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    return seed;
  }
};

} // namespace flux
