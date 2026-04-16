#pragma once

/// \file Flux/Scene/NodeStore.hpp
///
/// Part of the Flux public API.


#include <Flux/Scene/NodeId.hpp>
#include <Flux/Scene/Nodes.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace flux {

class NodeStore {
public:
  NodeId insert(SceneNode node);
  void remove(NodeId id);
  SceneNode* get(NodeId id);
  SceneNode const* get(NodeId id) const;
  bool contains(NodeId id) const;
  NodeId parentOf(NodeId id) const;
  void setParent(NodeId id, NodeId parent);
  std::uint64_t paintEpochOf(NodeId id) const;
  void setPaintEpoch(NodeId id, std::uint64_t epoch);

private:
  struct Slot {
    std::uint32_t generation = 0;
    NodeId parent{};
    std::uint64_t paintEpoch = 0;
    std::optional<SceneNode> node;
  };

  std::vector<Slot> slots_{};
  std::vector<std::size_t> free_{};
};

} // namespace flux
