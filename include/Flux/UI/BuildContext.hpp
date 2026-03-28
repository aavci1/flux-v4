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
class ComponentRegistry;

class BuildContext {
public:
  NodeId parentLayer() const;

  SceneGraph& graph();
  EventMap& eventMap();
  TextSystem& textSystem();
  LayoutEngine& layoutEngine();
  ComponentRegistry& registry();

  void pushLayer(NodeId layerId);
  void popLayer();

  LayoutConstraints const& constraints() const;
  void pushConstraints(LayoutConstraints const& c);
  void popConstraints();

  /// Returns the structural key prefix for the subtree currently being built.
  ComponentKey const& currentKey() const;

  /// Layout containers call these around each child build.
  void pushChildIndex();
  void popChildIndex();

  /// Composite component elements call this to resolve and advance the key.
  ComponentKey nextCompositeKey();

  /// Leaf elements and spacers advance the structural child index without registry lookup.
  void advanceChildSlot();

private:
  friend class Runtime;

  BuildContext(SceneGraph& g, EventMap& em, TextSystem& ts, LayoutEngine& layout,
                 ComponentRegistry& registry);

  SceneGraph& graph_;
  EventMap& eventMap_;
  TextSystem& textSystem_;
  LayoutEngine& layoutEngine_;
  ComponentRegistry& registry_;
  std::vector<NodeId> layerStack_;
  std::vector<LayoutConstraints> constraintStack_;
  std::vector<std::size_t> keyStack_;
  std::vector<std::size_t> savedChildIndices_;
  std::size_t nextChildIndex_{0};
};

} // namespace flux
