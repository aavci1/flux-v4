#pragma once

/// \file Flux/Graphics/TextCacheStats.hpp
///
/// Diagnostics for the Core Text caching layers (see TextSystem::stats()).

#include <cstdint>

namespace flux {

struct TextCacheStats {
  struct LayerStats {
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t evictions = 0;
    std::uint64_t currentBytes = 0;
    std::uint64_t peakBytes = 0;
  };

  LayerStats l0_sizedFont{};
  LayerStats l1_color{};
  LayerStats l1_runAttr{};
  LayerStats l1_paraStyle{};
  LayerStats l2_framesetter{};
  LayerStats l3_layout{};
  LayerStats l4_boxLayout{};

  std::uint64_t contentHashCollisions = 0;
};

} // namespace flux
