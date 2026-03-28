#include <Flux/Scene/NodeStore.hpp>

#include <utility>
#include <variant>

namespace flux {

NodeId NodeStore::insert(SceneNode node) {
  std::size_t idx;
  if (!free_.empty()) {
    idx = free_.back();
    free_.pop_back();
  } else {
    slots_.push_back({});
    idx = slots_.size() - 1;
  }
  Slot& s = slots_[idx];
  if (s.generation == 0) {
    s.generation = 1;
  }
  const NodeId id{static_cast<std::uint32_t>(idx), s.generation};
  std::visit([&](auto& n) { n.id = id; }, node);
  s.node = std::move(node);
  return id;
}

void NodeStore::remove(NodeId id) {
  if (!contains(id)) {
    return;
  }
  Slot& s = slots_[id.index];
  s.node = std::nullopt;
  ++s.generation;
  free_.push_back(static_cast<std::size_t>(id.index));
}

SceneNode* NodeStore::get(NodeId id) {
  if (id.index >= slots_.size()) {
    return nullptr;
  }
  Slot& s = slots_[id.index];
  if (!s.node.has_value() || s.generation != id.generation) {
    return nullptr;
  }
  return &*s.node;
}

SceneNode const* NodeStore::get(NodeId id) const {
  if (id.index >= slots_.size()) {
    return nullptr;
  }
  Slot const& s = slots_[id.index];
  if (!s.node.has_value() || s.generation != id.generation) {
    return nullptr;
  }
  return &*s.node;
}

bool NodeStore::contains(NodeId id) const { return get(id) != nullptr; }

} // namespace flux
