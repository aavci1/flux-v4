#pragma once

/// \file Flux/Layout/LayoutHints.hpp
///
/// Cross-axis alignment hints carried through retained-scene measurement.

#include <Flux/Layout/Alignment.hpp>

#include <optional>

namespace flux {

struct LayoutHints {
  std::optional<Alignment> hStackCrossAlign;
  std::optional<Alignment> vStackCrossAlign;
  std::optional<Alignment> zStackHorizontalAlign;
  std::optional<Alignment> zStackVerticalAlign;
};

} // namespace flux
