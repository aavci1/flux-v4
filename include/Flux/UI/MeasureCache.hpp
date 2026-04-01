#pragma once

/// \file Flux/UI/MeasureCache.hpp
///
/// Part of the Flux public API.


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

bool operator==(MeasureCacheKey const& a, MeasureCacheKey const& b) noexcept;

struct MeasureCacheKeyHash {
  std::size_t operator()(MeasureCacheKey const& k) const noexcept;
};

MeasureCacheKey makeMeasureCacheKey(std::uint64_t elementMeasureId, LayoutConstraints const& c);

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
  void clear();

  std::optional<Size> tryGet(MeasureCacheKey const& key) const;

  void put(MeasureCacheKey const& key, Size size);

private:
  std::unordered_map<MeasureCacheKey, Size, MeasureCacheKeyHash> map_{};
};

} // namespace flux
