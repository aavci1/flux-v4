#include <Flux/UI/EventMap.hpp>

namespace flux {

std::size_t NodeIdHash::operator()(NodeId id) const noexcept {
  return static_cast<std::size_t>(id.index) ^ (static_cast<std::size_t>(id.generation) << 16u);
}

void EventMap::insert(NodeId id, EventHandlers handlers) { map_[id] = std::move(handlers); }

EventHandlers const* EventMap::find(NodeId id) const {
  auto it = map_.find(id);
  if (it == map_.end()) {
    return nullptr;
  }
  return &it->second;
}

void EventMap::clear() { map_.clear(); }

} // namespace flux
