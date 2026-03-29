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
  /// During an active drag (after `onPointerDown`), move events are routed to the
  /// pressed node when it has `onPointerMove`, using coordinates in that node's space
  /// even if the pointer leaves the node's bounds. Outside a drag, events go to the
  /// node under the pointer.
  std::function<void(Point)> onPointerMove;
  std::function<void(Vec2)> onScroll;
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
