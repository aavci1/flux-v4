#pragma once

#include <Flux/Scene/NodeId.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <cstddef>
#include <vector>

namespace flux {

class SceneGraph;
class TextSystem;
class EventMap;
class Runtime;

class BuildContext {
public:
  NodeId parentLayer() const;

  SceneGraph& graph();
  EventMap& eventMap();
  TextSystem& textSystem();
  LayoutEngine& layoutEngine();

  void pushLayer(NodeId layerId);
  void popLayer();

  LayoutConstraints const& constraints() const;
  void pushConstraints(LayoutConstraints const& c);
  void popConstraints();

  /// Called by layout containers around their child build loops.
  void pushChildIndex();
  void popChildIndex();

  /// Called by Element::Model<C>::build for composite components.
  /// Returns the key for this component and advances the cursor.
  ComponentKey nextCompositeKey();

  /// Called by leaf build specialisations and Spacer.
  void advanceChildSlot();

  /// After the measure pass over children, reset the per-container child index so the
  /// build pass assigns the same composite keys (pairs with resetSlotCursors).
  void rewindChildKeyIndex();

  /// Model<C>::build calls this before child.build — the subtree root must not consume a
  /// second sibling index after nextCompositeKey.
  void beginCompositeBodySubtree();
  /// Returns true if this build should skip advanceChildSlot (composite body subtree root).
  bool consumeCompositeBodySubtreeRootSkip();

private:
  friend class Runtime;

  BuildContext(SceneGraph& g, EventMap& em, TextSystem& ts, LayoutEngine& layout);

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
};

} // namespace flux
