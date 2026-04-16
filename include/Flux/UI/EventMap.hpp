#pragma once

/// \file Flux/UI/EventMap.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Scene/NodeId.hpp>
#include <Flux/UI/ComponentKey.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace flux {

class SceneGraph;

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

  Cursor cursor = Cursor::Inherit;
};

struct NodeIdHash {
  std::size_t operator()(NodeId id) const noexcept;
};

class EventMap {
public:
  void insert(NodeId id, EventHandlers handlers);
  void remove(NodeId id);
  EventHandlers const* find(NodeId id) const;
  /// First entry whose `stableTargetKey` matches \p key (O(n)); used when `NodeId` is stale after rebuild.
  std::pair<NodeId, EventHandlers const*> findWithIdByKey(ComponentKey const& key) const;
  /// Best-effort fallback when the exact `stableTargetKey` changed across a rebuild.
  /// Returns the nearest ancestor/descendant relation to \p key (never a sibling/cousin).
  std::pair<NodeId, EventHandlers const*> findClosestWithIdByKey(ComponentKey const& key) const;
  void clear();
  void prune(SceneGraph const& graph);

  /// Stable keys of all focusable nodes, in build order.
  std::vector<ComponentKey> const& focusOrder() const;

private:
  std::unordered_map<NodeId, EventHandlers, NodeIdHash> map_;
  std::vector<ComponentKey> focusOrder_{};
};

} // namespace flux
