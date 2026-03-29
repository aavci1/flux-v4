#include <Flux/UI/EventMap.hpp>

namespace flux {

std::size_t NodeIdHash::operator()(NodeId id) const noexcept {
  return static_cast<std::size_t>(id.index) ^ (static_cast<std::size_t>(id.generation) << 16u);
}

void EventMap::insert(NodeId id, EventHandlers handlers) {
  if (handlers.focusable && !handlers.stableTargetKey.empty()) {
    focusOrder_.push_back(handlers.stableTargetKey);
  }
  map_.insert_or_assign(id, std::move(handlers));
}

EventHandlers const* EventMap::find(NodeId id) const {
  auto it = map_.find(id);
  if (it == map_.end()) {
    return nullptr;
  }
  return &it->second;
}

std::pair<NodeId, EventHandlers const*> EventMap::findWithIdByKey(ComponentKey const& key) const {
  if (key.empty()) {
    return {kInvalidNodeId, nullptr};
  }
  for (auto const& [id, h] : map_) {
    if (h.stableTargetKey == key) {
      return {id, &h};
    }
  }
  return {kInvalidNodeId, nullptr};
}

void EventMap::clear() {
  map_.clear();
  focusOrder_.clear();
}

} // namespace flux
