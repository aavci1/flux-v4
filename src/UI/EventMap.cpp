#include <Flux/UI/EventMap.hpp>

#include <algorithm>
#include <cstddef>

namespace flux {

namespace {

bool isPrefix(ComponentKey const& prefix, ComponentKey const& key) {
  return prefix.size() <= key.size() && std::equal(prefix.begin(), prefix.end(), key.begin());
}

} // namespace

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

std::pair<NodeId, EventHandlers const*> EventMap::findClosestWithIdByKey(ComponentKey const& key) const {
  auto const exact = findWithIdByKey(key);
  if (exact.second || key.empty()) {
    return exact;
  }

  NodeId bestId = kInvalidNodeId;
  EventHandlers const* bestHandlers = nullptr;
  std::size_t bestSharedDepth = 0;
  std::size_t bestDistance = 0;

  for (auto const& [id, h] : map_) {
    if (h.stableTargetKey.empty()) {
      continue;
    }
    bool const candidateIsAncestor = isPrefix(h.stableTargetKey, key);
    bool const candidateIsDescendant = isPrefix(key, h.stableTargetKey);
    if (!candidateIsAncestor && !candidateIsDescendant) {
      continue;
    }

    std::size_t const sharedDepth = std::min(h.stableTargetKey.size(), key.size());
    std::size_t const distance =
        h.stableTargetKey.size() > key.size() ? (h.stableTargetKey.size() - key.size())
                                              : (key.size() - h.stableTargetKey.size());

    if (!bestHandlers || sharedDepth > bestSharedDepth ||
        (sharedDepth == bestSharedDepth && distance < bestDistance)) {
      bestId = id;
      bestHandlers = &h;
      bestSharedDepth = sharedDepth;
      bestDistance = distance;
    }
  }

  return {bestId, bestHandlers};
}

void EventMap::clear() {
  map_.clear();
  focusOrder_.clear();
}

std::vector<ComponentKey> const& EventMap::focusOrder() const {
  return focusOrder_;
}

} // namespace flux
