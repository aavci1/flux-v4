#pragma once

/// \file Flux/Reactive/Detail/ReactiveBuildGate.hpp
///
/// Tracks reactive content changes for coalescing redundant UI rebuilds (see `Runtime::rebuild`).

#include <cstdint>

namespace flux::detail {

/// Bumped when any `Signal` value or `Animated` value used for UI output changes.
void bumpReactiveContentEpoch();
[[nodiscard]] std::uint64_t reactiveContentEpoch() noexcept;

/// Set when `Signal`/`Animated` mutates while a `StateStore` build pass is active (layout wrote state).
void markStateMutationDuringBuildIfActive();
[[nodiscard]] bool peekStateMutationDuringBuild() noexcept;
void clearStateMutationDuringBuild() noexcept;

} // namespace flux::detail
