#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <cstring>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>

namespace flux {

/// Key for memoizing leaf \ref Element::measure results within a single layout pass.
/// Uses \ref Element's unique `measureId` (not `impl` pointer): allocators can reuse the same
/// heap address after a temporary \ref Element is destroyed (e.g. \ref ForEach measure loop),
/// which would otherwise collide in the cache. The key does not hash text or other mutable
/// content — correctness relies on one `measureId` per distinct element instance and on
/// \ref MeasureCache being cleared at each rebuild start.
struct MeasureCacheKey {
  std::uint64_t elementMeasureId{};
  float maxWidth{};
  float maxHeight{};
  float minWidth{};
  float minHeight{};
  std::uint8_t hStackCross{};
  std::uint8_t vStackCross{};
};

inline bool operator==(MeasureCacheKey const& a, MeasureCacheKey const& b) noexcept {
  return a.elementMeasureId == b.elementMeasureId && a.maxWidth == b.maxWidth &&
         a.maxHeight == b.maxHeight && a.minWidth == b.minWidth && a.minHeight == b.minHeight &&
         a.hStackCross == b.hStackCross && a.vStackCross == b.vStackCross;
}

struct MeasureCacheKeyHash {
  std::size_t operator()(MeasureCacheKey const& k) const noexcept {
    std::size_t h = std::hash<std::uint64_t>{}(k.elementMeasureId);
    auto mixFloat = [&](float f) {
      std::uint32_t u{};
      static_assert(sizeof(f) == sizeof(u));
      std::memcpy(&u, &f, sizeof(u));
      h ^= static_cast<std::size_t>(u + 0x9e3779b9u + (h << 6) + (h >> 2));
    };
    mixFloat(k.maxWidth);
    mixFloat(k.maxHeight);
    mixFloat(k.minWidth);
    mixFloat(k.minHeight);
    h ^= static_cast<std::size_t>(k.hStackCross + 0x9e3779b9u + (h << 6) + (h >> 2));
    h ^= static_cast<std::size_t>(k.vStackCross + 0x9e3779b9u + (h << 6) + (h >> 2));
    return h;
  }
};

inline MeasureCacheKey makeMeasureCacheKey(std::uint64_t elementMeasureId, LayoutConstraints const& c) {
  MeasureCacheKey k{};
  k.elementMeasureId = elementMeasureId;
  k.maxWidth = c.maxWidth;
  k.maxHeight = c.maxHeight;
  k.minWidth = c.minWidth;
  k.minHeight = c.minHeight;
  k.hStackCross = c.hStackCrossAlign
      ? static_cast<std::uint8_t>(*c.hStackCrossAlign) + std::uint8_t{1}
      : std::uint8_t{0};
  k.vStackCross = c.vStackCrossAlign
      ? static_cast<std::uint8_t>(*c.vStackCrossAlign) + std::uint8_t{1}
      : std::uint8_t{0};
  return k;
}

/// Per-pass cache. Cleared when a new rebuild starts so content changes always get fresh measures;
/// within one pass, distinct elements have distinct `measureId`s so there is no stale hit from
/// differing state alone.
///
/// The key does not include mutable leaf content (e.g. `State<std::string>`). Memoization is
/// correct while no `State` writes occur during the measure pass and nothing replays `measure`
/// after `build()` mutates state in the same pass — an implicit invariant today. A leaf whose
/// `measure` reads reactive state and could become incorrect if that invariant breaks should
/// return `canMemoizeMeasure() == false`, or the engine would need a cache flush between measure
/// and build.
class MeasureCache {
public:
  void clear() { map_.clear(); }

  std::optional<Size> tryGet(MeasureCacheKey const& key) const {
    auto const it = map_.find(key);
    if (it == map_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  void put(MeasureCacheKey const& key, Size size) { map_.insert_or_assign(key, size); }

private:
  std::unordered_map<MeasureCacheKey, Size, MeasureCacheKeyHash> map_{};
};

} // namespace flux
