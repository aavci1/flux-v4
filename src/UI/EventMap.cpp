#include <Flux/UI/EventMap.hpp>

#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include <algorithm>
#include <vector>

namespace flux {

namespace {

void collectSubtreeNodeIds(SceneGraph const& graph, NodeId id, std::vector<NodeId>& out) {
  out.push_back(id);
  if (auto const* layer = graph.node<LayerNode>(id)) {
    for (NodeId c : layer->children) {
      collectSubtreeNodeIds(graph, c, out);
    }
  }
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

void EventMap::removeSubtree(SceneGraph const& graph, NodeId subtreeRoot) {
  std::vector<NodeId> nodes;
  collectSubtreeNodeIds(graph, subtreeRoot, nodes);
  for (NodeId id : nodes) {
    auto it = map_.find(id);
    if (it == map_.end()) {
      continue;
    }
    EventHandlers const& h = it->second;
    if (h.focusable && !h.stableTargetKey.empty()) {
      std::erase(focusOrder_, h.stableTargetKey);
    }
    map_.erase(it);
  }
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

std::vector<ComponentKey> const& EventMap::focusOrder() const {
  return focusOrder_;
}

} // namespace flux
