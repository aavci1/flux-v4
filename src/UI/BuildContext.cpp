#include <Flux/UI/BuildContext.hpp>

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/UI/EventMap.hpp>

namespace flux {

BuildContext::BuildContext(SceneGraph& g, EventMap& em, TextSystem& ts, LayoutEngine& layout)
    : graph_(g), eventMap_(em), textSystem_(ts), layoutEngine_(layout) {
  constraintStack_.push_back(LayoutConstraints{});
}

NodeId BuildContext::parentLayer() const {
  if (layerStack_.empty()) {
    return graph_.root();
  }
  return layerStack_.back();
}

SceneGraph& BuildContext::graph() { return graph_; }

EventMap& BuildContext::eventMap() { return eventMap_; }

TextSystem& BuildContext::textSystem() { return textSystem_; }

LayoutEngine& BuildContext::layoutEngine() { return layoutEngine_; }

void BuildContext::pushLayer(NodeId layerId) { layerStack_.push_back(layerId); }

void BuildContext::popLayer() {
  if (!layerStack_.empty()) {
    layerStack_.pop_back();
  }
}

LayoutConstraints const& BuildContext::constraints() const { return constraintStack_.back(); }

void BuildContext::pushConstraints(LayoutConstraints const& c) { constraintStack_.push_back(c); }

void BuildContext::popConstraints() {
  if (constraintStack_.size() > 1) {
    constraintStack_.pop_back();
  }
}

} // namespace flux
