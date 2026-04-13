#include <Flux/UI/RenderContext.hpp>

#include <cassert>
#include <cmath>

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/UI/EventMap.hpp>

namespace flux {

RenderContext::RenderContext(SceneGraph& g, EventMap& em, TextSystem& ts)
    : graph_(g)
    , eventMap_(em)
    , textSystem_(ts) {
  layoutStack_.push_back(LayoutFrame{});
}

SceneGraph& RenderContext::graph() { return graph_; }

EventMap& RenderContext::eventMap() { return eventMap_; }

TextSystem& RenderContext::textSystem() { return textSystem_; }

NodeId RenderContext::parentLayer() const {
  if (layerStack_.empty()) {
    return graph_.root();
  }
  return layerStack_.back();
}

void RenderContext::pushLayer(NodeId layerId) { layerStack_.push_back(layerId); }

void RenderContext::popLayer() {
  if (!layerStack_.empty()) {
    layerStack_.pop_back();
  }
}

LayoutConstraints const& RenderContext::constraints() const { return layoutStack_.back().constraints; }

LayoutHints const& RenderContext::hints() const { return layoutStack_.back().hints; }

void RenderContext::pushConstraints(LayoutConstraints const& c, LayoutHints hints) {
#ifndef NDEBUG
  assert(std::isfinite(c.minWidth) && std::isfinite(c.minHeight));
  assert(c.minWidth <= c.maxWidth);
  assert(c.minHeight <= c.maxHeight);
#endif
  layoutStack_.push_back(LayoutFrame{.constraints = c, .hints = std::move(hints)});
}

void RenderContext::popConstraints() {
  if (layoutStack_.size() > 1) {
    layoutStack_.pop_back();
  }
}

void RenderContext::pushActiveElementModifiers(ElementModifiers const* m) { activeElementModifiers_.push_back(m); }

void RenderContext::popActiveElementModifiers() {
#ifndef NDEBUG
  assert(!activeElementModifiers_.empty());
#endif
  activeElementModifiers_.pop_back();
}

ElementModifiers const* RenderContext::activeElementModifiers() const noexcept {
  return activeElementModifiers_.empty() ? nullptr : activeElementModifiers_.back();
}

void RenderContext::pushSuppressLeafModifierEvents(bool suppress) { suppressLeafModifierEvents_.push_back(suppress); }

void RenderContext::popSuppressLeafModifierEvents() {
#ifndef NDEBUG
  assert(!suppressLeafModifierEvents_.empty());
#endif
  suppressLeafModifierEvents_.pop_back();
}

bool RenderContext::suppressLeafModifierEvents() const noexcept {
  return !suppressLeafModifierEvents_.empty() && suppressLeafModifierEvents_.back();
}

void RenderContext::bindContainerLayerRegistry(
    std::unordered_map<ComponentKey, NodeId, ComponentKeyHash>* registry) {
  containerLayerRegistry_ = registry;
}

void RenderContext::registerContainerLayer(ComponentKey const& key, NodeId id) {
  if (!containerLayerRegistry_ || key.empty() || !id.isValid()) {
    return;
  }
  (*containerLayerRegistry_)[key] = id;
}

} // namespace flux
