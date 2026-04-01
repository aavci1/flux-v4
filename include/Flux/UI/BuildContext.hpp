#pragma once

/// \file Flux/UI/BuildContext.hpp
///
/// Part of the Flux public API.


#include <Flux/Scene/NodeId.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace flux {

class SceneGraph;
class TextSystem;
class EventMap;
class Runtime;
class BuildOrchestrator;
class MeasureCache;

class BuildContext {
public:
  NodeId parentLayer() const;

  SceneGraph& graph();
  EventMap& eventMap();
  TextSystem& textSystem();
  LayoutEngine& layoutEngine();
  /// Non-null during main and overlay rebuilds; holds leaf measure memoization for the current pass.
  MeasureCache* measureCache() const;

  void pushLayer(NodeId layerId);
  void popLayer();

  LayoutConstraints const& constraints() const;
  void pushConstraints(LayoutConstraints const& c);
  void popConstraints();

  /// Called by layout containers around their child build loops.
  void pushChildIndex();
  void popChildIndex();

  /// Override the next leaf/composite index within the current key segment (used by `ForEach`).
  void setChildIndex(std::size_t index);

  /// Called by Element::Model<C>::build for composite components.
  /// Returns the key for this component and advances the cursor.
  ComponentKey nextCompositeKey();

  /// Called by leaf build specialisations and Spacer.
  void advanceChildSlot();

  /// Structural path for the next leaf slot (call before `advanceChildSlot` for that leaf).
  ComponentKey leafComponentKey() const;

  /// After the measure pass over children, reset the per-container child index so the
  /// build pass assigns the same composite keys (pairs with resetSlotCursors).
  void rewindChildKeyIndex();

  /// Model<C>::build calls this before child.build — the subtree root must not consume a
  /// second sibling index after nextCompositeKey. \p compositeKey is the composite whose `body()`
  /// subtree is being built (for `useLayoutRect`).
  void beginCompositeBodySubtree(ComponentKey compositeKey);
  /// Returns true if this build should skip advanceChildSlot (composite body subtree root).
  bool consumeCompositeBodySubtreeRootSkip();

  /// After `nextCompositeKey()`, push the composite segment onto `keyStack_` and reset
  /// `nextChildIndex_` so inner layouts (VStack, etc.) use correct leaf keys extending this
  /// composite path. Paired with `popCompositeKeyTail()` after `child.build` / `child.measure`.
  void pushCompositeKeyTail(ComponentKey const& compositeKey);
  void popCompositeKeyTail();

  /// First `addLayer` after `beginCompositeBodySubtree` records the subtree root for `useLayoutRect`.
  void registerCompositeSubtreeRootIfPending(NodeId layerId);

  std::unordered_map<ComponentKey, NodeId, ComponentKeyHash> const& subtreeRootLayers() const;

private:
  friend class Runtime;
  friend class BuildOrchestrator;
  friend class OverlayManager;

  BuildContext(SceneGraph& g, EventMap& em, TextSystem& ts, LayoutEngine& layout,
               MeasureCache* measureCache = nullptr);

  SceneGraph& graph_;
  EventMap& eventMap_;
  TextSystem& textSystem_;
  LayoutEngine& layoutEngine_;
  std::vector<NodeId> layerStack_;
  std::vector<LayoutConstraints> constraintStack_;

  std::vector<std::size_t> keyStack_;
  std::vector<std::size_t> savedChildIndices_;
  std::size_t nextChildIndex_{0};
  bool skipNextLayoutChildAdvance_{false};
  bool pendingCompositeSubtreeRoot_{false};
  ComponentKey pendingCompositeSubtreeKey_{};
  std::unordered_map<ComponentKey, NodeId, ComponentKeyHash> subtreeRootLayers_{};
  MeasureCache* measureCache_{nullptr};
};

} // namespace flux
