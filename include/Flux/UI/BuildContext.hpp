#pragma once

#include <Flux/Scene/NodeId.hpp>
#include <Flux/UI/LayoutEngine.hpp>

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

private:
  friend class Runtime;

  BuildContext(SceneGraph& g, EventMap& em, TextSystem& ts, LayoutEngine& layout);

  SceneGraph& graph_;
  EventMap& eventMap_;
  TextSystem& textSystem_;
  LayoutEngine& layoutEngine_;
  std::vector<NodeId> layerStack_;
  std::vector<LayoutConstraints> constraintStack_;
};

} // namespace flux
