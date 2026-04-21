#pragma once

/// \file Flux/UI/LayoutEngine.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/UI/Alignment.hpp>

#include <limits>
#include <optional>

namespace flux {

/// Cross-axis alignment propagated by stacks — not size constraints; carried beside
/// \ref LayoutConstraints during retained-scene measurement/build.
struct LayoutHints {
  /// Set by `HStack` for each row child (`HStack::alignment`). Used by `resolveLeafLayoutBounds` and
  /// similar. Cleared by `VStack` when building children so it does not leak into nested rows.
  std::optional<Alignment> hStackCrossAlign;
  /// Set by `VStack` / `ForEach` for each row (`VStack::alignment`). `Text` maps to
  /// \ref TextLayoutOptions in `Element.cpp`. Cleared by `HStack`, `Grid`, `OffsetView`, `ZStack`.
  std::optional<Alignment> vStackCrossAlign;
  /// Set by `ZStack` for direct children. `Stretch` means the child owns the full shared slot on
  /// the horizontal axis; the other alignments mean the child should shrink-wrap first and be
  /// offset afterward.
  std::optional<Alignment> zStackHorizontalAlign;
  /// Set by `ZStack` for direct children. `Stretch` means the child owns the full shared slot on
  /// the vertical axis; the other alignments mean the child should shrink-wrap first and be offset
  /// afterward.
  std::optional<Alignment> zStackVerticalAlign;
};

struct LayoutConstraints {
  float maxWidth = std::numeric_limits<float>::infinity();
  float maxHeight = std::numeric_limits<float>::infinity();
  float minWidth = 0.f;
  float minHeight = 0.f;
};

} // namespace flux
