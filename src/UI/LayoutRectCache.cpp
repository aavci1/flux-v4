#include <Flux/UI/LayoutRectCache.hpp>

#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/LayoutTree.hpp>
#include <Flux/UI/StateStore.hpp>

namespace flux {

void LayoutRectCache::fill(LayoutTree const& tree, LayoutContext const& ctx) {
  prev_.swap(current_);
  current_.clear();
  for (auto const& [key, nodeId] : ctx.subtreeRootLayouts()) {
    if (nodeId.isValid()) {
      current_[key] = tree.unionSubtreeWorldBounds(nodeId);
    }
  }
}

std::optional<Rect> LayoutRectCache::forCurrentComponent(StateStore const& store) const {
  ComponentKey const& key = store.currentComponentKey();
  if (auto it = current_.find(key); it != current_.end()) {
    return it->second;
  }
  if (auto it = prev_.find(key); it != prev_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<Rect> LayoutRectCache::forKey(ComponentKey const& key) const {
  if (auto it = current_.find(key); it != current_.end()) {
    return it->second;
  }
  if (auto it = prev_.find(key); it != prev_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<Rect> LayoutRectCache::forLeafKeyPrefix(ComponentKey const& key) const {
  for (std::size_t len = key.size(); len > 0; --len) {
    ComponentKey prefix(key.begin(), key.begin() + static_cast<std::ptrdiff_t>(len));
    if (auto it = current_.find(prefix); it != current_.end()) {
      return it->second;
    }
    if (auto it = prev_.find(prefix); it != prev_.end()) {
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
