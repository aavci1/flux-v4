#include <Flux/UI/LayoutRectCache.hpp>

#include <Flux/Scene/SceneGraphBounds.hpp>
#include <Flux/UI/StateStore.hpp>

namespace flux {

void LayoutRectCache::fill(SceneGraph const& graph, BuildContext const& ctx) {
  prev_.swap(current_);
  current_.clear();
  for (auto const& [key, nodeId] : ctx.subtreeRootLayers()) {
    Mat3 const pw = subtreeAncestorWorldTransform(graph, nodeId);
    current_[key] = unionSubtreeBounds(graph, nodeId, pw);
  }
}

std::optional<Rect> LayoutRectCache::forCurrentComponent(StateStore const& store) const {
  auto it = current_.find(store.currentComponentKey());
  if (it == current_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<Rect> LayoutRectCache::forKey(ComponentKey const& key) const {
  auto it = current_.find(key);
  if (it == current_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<Rect> LayoutRectCache::forLeafKeyPrefix(ComponentKey const& key) const {
  for (std::size_t len = key.size(); len > 0; --len) {
    ComponentKey prefix(key.begin(), key.begin() + static_cast<std::ptrdiff_t>(len));
    if (auto it = current_.find(prefix); it != current_.end()) {
      return it->second;
    }
  }
  return std::nullopt;
}

std::optional<Rect> LayoutRectCache::forTapAnchor(ComponentKey const& tapLeafKey) const {
  if (tapLeafKey.empty()) {
    return std::nullopt;
  }
  return forLeafKeyPrefix(tapLeafKey);
}

} // namespace flux
