#include <Flux/UI/MeasureCache.hpp>

#include <cstring>
#include <functional>
#include <optional>

namespace flux {

bool operator==(MeasureCacheKey const& a, MeasureCacheKey const& b) noexcept {
  return a.elementIdentity == b.elementIdentity && a.maxWidth == b.maxWidth &&
         a.maxHeight == b.maxHeight && a.minWidth == b.minWidth && a.minHeight == b.minHeight &&
         a.hStackCross == b.hStackCross && a.vStackCross == b.vStackCross;
}

std::size_t MeasureCacheKeyHash::operator()(MeasureCacheKey const& k) const noexcept {
  std::size_t h = std::hash<std::uint64_t>{}(k.elementIdentity);
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

MeasureCacheKey makeMeasureCacheKey(std::uint64_t elementIdentity, LayoutConstraints const& c,
                                    LayoutHints const& h) {
  MeasureCacheKey k{};
  k.elementIdentity = elementIdentity;
  k.maxWidth = c.maxWidth;
  k.maxHeight = c.maxHeight;
  k.minWidth = c.minWidth;
  k.minHeight = c.minHeight;
  k.hStackCross = h.hStackCrossAlign
      ? static_cast<std::uint8_t>(*h.hStackCrossAlign) + std::uint8_t{1}
      : std::uint8_t{0};
  k.vStackCross = h.vStackCrossAlign
      ? static_cast<std::uint8_t>(*h.vStackCrossAlign) + std::uint8_t{1}
      : std::uint8_t{0};
  return k;
}

void MeasureCache::beginBuild(bool forceFullRebuild) {
  ++buildEpoch_;
  if (forceFullRebuild) {
    map_.clear();
    return;
  }
  if (buildEpoch_ <= kRetainBuilds) {
    return;
  }
  std::uint64_t const cutoff = buildEpoch_ - kRetainBuilds;
  for (auto it = map_.begin(); it != map_.end();) {
    if (it->second.lastUsedEpoch < cutoff) {
      it = map_.erase(it);
    } else {
      ++it;
    }
  }
}

void MeasureCache::clear() {
  map_.clear();
}

std::optional<Size> MeasureCache::tryGet(MeasureCacheKey const& key) {
  auto const it = map_.find(key);
  if (it == map_.end()) {
    return std::nullopt;
  }
  it->second.lastUsedEpoch = buildEpoch_;
  return it->second.size;
}

void MeasureCache::put(MeasureCacheKey const& key, Size size) {
  map_.insert_or_assign(key, Entry{.size = size, .lastUsedEpoch = buildEpoch_});
}

} // namespace flux
