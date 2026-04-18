#include <Flux/UI/SceneGeometryIndex.hpp>

#include <Flux/UI/StateStore.hpp>

namespace flux {

namespace {

bool keyHasPrefix(ComponentKey const& key, ComponentKey const& prefix) {
  if (prefix.empty()) {
    return true;
  }
  if (key.size() < prefix.size()) {
    return false;
  }
  return std::equal(prefix.begin(), prefix.end(), key.begin());
}

Rect offsetRect(Rect rect, Point delta) {
  rect.x += delta.x;
  rect.y += delta.y;
  return rect;
}

} // namespace

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

void SceneGeometryIndex::retainSubtree(ComponentKey const& key, Point delta) {
  auto copyMatches = [&](auto const& source) {
    for (auto const& [candidateKey, rect] : source) {
      if (!keyHasPrefix(candidateKey, key)) {
        continue;
      }
      building_.try_emplace(candidateKey, offsetRect(rect, delta));
    }
  };
  copyMatches(current_);
  copyMatches(prev_);
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

std::vector<std::pair<ComponentKey, Rect>> SceneGeometryIndex::snapshotCurrent() const {
  std::vector<std::pair<ComponentKey, Rect>> out;
  out.reserve(current_.size());
  for (auto const& [key, rect] : current_) {
    out.emplace_back(key, rect);
  }
  return out;
}

} // namespace flux
