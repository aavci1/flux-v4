#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Scene/NodeId.hpp>
#include <Flux/UI/ComponentKey.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

namespace flux {

struct EventHandlers {
  /// When non-empty, identifies this logical target across scene rebuilds (same subtree path).
  ComponentKey stableTargetKey;

  std::function<void()> onTap;
  std::function<void(Point)> onPointerDown;
  std::function<void(Point)> onPointerUp;
  /// During an active drag (after `onPointerDown`), move events are routed to the
  /// pressed node when it has `onPointerMove`, using coordinates in that node's space
  /// even if the pointer leaves the node's bounds. Outside a drag, events go to the
  /// node under the pointer.
  std::function<void(Point)> onPointerMove;
  std::function<void(Vec2)> onScroll;
  std::function<void(KeyCode, Modifiers)> onKeyDown;
  std::function<void(KeyCode, Modifiers)> onKeyUp;
  std::function<void(std::string const&)> onTextInput;
  /// True when the view sets `focusable` or registers any keyboard handler (used for focus claim).
  bool focusable = false;
};

struct NodeIdHash {
  std::size_t operator()(NodeId id) const noexcept;
};

class EventMap {
public:
  void insert(NodeId id, EventHandlers handlers);
  EventHandlers const* find(NodeId id) const;
  /// First entry whose `stableTargetKey` matches \p key (O(n)); used when `NodeId` is stale after rebuild.
  std::pair<NodeId, EventHandlers const*> findWithIdByKey(ComponentKey const& key) const;
  void clear();

private:
  std::unordered_map<NodeId, EventHandlers, NodeIdHash> map_;
};

} // namespace flux
