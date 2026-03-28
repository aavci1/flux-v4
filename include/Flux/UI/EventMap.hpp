#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Scene/NodeId.hpp>

#include <cstddef>
#include <functional>
#include <unordered_map>

namespace flux {

struct EventHandlers {
  std::function<void()> onTap;
  std::function<void(Point)> onPointerDown;
  std::function<void(Point)> onPointerUp;
  std::function<void(Point)> onPointerMove;
};

struct NodeIdHash {
  std::size_t operator()(NodeId id) const noexcept;
};

class EventMap {
public:
  void insert(NodeId id, EventHandlers handlers);
  EventHandlers const* find(NodeId id) const;
  void clear();

private:
  std::unordered_map<NodeId, EventHandlers, NodeIdHash> map_;
};

} // namespace flux
