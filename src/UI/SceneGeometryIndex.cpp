#include <Flux/UI/SceneGeometryIndex.hpp>

#include <Flux/UI/StateStore.hpp>

namespace flux {

void SceneGeometryIndex::finishBuild() {
  prev_.swap(current_);
  current_ = std::move(building_);
  building_.clear();
}

void SceneGeometryIndex::clear() {
  current_.clear();
  prev_.clear();
  building_.clear();
}

std::optional<Rect> SceneGeometryIndex::forCurrentComponent(StateStore const& store) const {
  return forKey(store.currentComponentKey());
}

std::optional<Rect> SceneGeometryIndex::forKey(ComponentKey const& key) const {
  if (auto it = current_.find(key); it != current_.end()) {
    return it->second;
  }
  if (auto it = prev_.find(key); it != prev_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<Rect> SceneGeometryIndex::forLeafKeyPrefix(ComponentKey const& stableTargetKey) const {
  for (std::size_t len = stableTargetKey.size(); len > 0; --len) {
    ComponentKey prefix(stableTargetKey.begin(), stableTargetKey.begin() + static_cast<std::ptrdiff_t>(len));
    if (std::optional<Rect> rect = forKey(prefix)) {
      return rect;
    }
  }
  return std::nullopt;
}

std::optional<Rect> SceneGeometryIndex::forTapAnchor(ComponentKey const& tapLeafKey) const {
  if (tapLeafKey.empty()) {
    return std::nullopt;
  }
  return forLeafKeyPrefix(tapLeafKey);
}

} // namespace flux
