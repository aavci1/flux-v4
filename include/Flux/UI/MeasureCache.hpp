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

/// Key for memoizing leaf \ref Element::measure results across retained reactive rebuilds.
/// The identity portion is a stable structural token when a memoizable leaf provides one, and
/// falls back to \ref Element's per-instance `measureId` otherwise. Cross-axis alignment from
/// \ref LayoutHints is folded into `hStackCross` / `vStackCross` bytes.
struct MeasureCacheKey {
  std::uint64_t elementIdentity{};
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

MeasureCacheKey makeMeasureCacheKey(std::uint64_t elementIdentity, LayoutConstraints const& c,
                                    LayoutHints const& h);

/// Cross-rebuild cache for memoizable leaves. A memoizable leaf that wants hits across rebuilt
/// \ref Element wrappers must provide a structural measure-cache key; otherwise the cache falls
/// back to \ref Element::measureId and only hits when the exact wrapper instance is retained.
class MeasureCache {
public:
  void beginBuild(bool forceFullRebuild);

  void clear();

  std::optional<Size> tryGet(MeasureCacheKey const& key);

  void put(MeasureCacheKey const& key, Size size);

private:
  struct Entry {
    Size size{};
    std::uint64_t lastUsedEpoch = 0;
  };

  std::unordered_map<MeasureCacheKey, Entry, MeasureCacheKeyHash> map_{};
  std::uint64_t buildEpoch_ = 0;
  static constexpr std::uint64_t kRetainBuilds = 8;
};

} // namespace flux
