#include <Flux/UI/RenderContext.hpp>

#include <cassert>
#include <cmath>

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/UI/EventMap.hpp>

namespace flux {

namespace {

void pushCapturedVector(void* storage, NodeId id) {
  static_cast<std::vector<NodeId>*>(storage)->push_back(id);
}

void pushCapturedSmallVector(void* storage, NodeId id) {
  static_cast<detail::SmallVector<NodeId, 2>*>(storage)->push_back(id);
}

} // namespace

RenderContext::RenderContext(SceneGraph& g, EventMap& em, TextSystem& ts, bool incrementalSceneReuse)
    : graph_(g)
    , eventMap_(em)
    , textSystem_(ts)
    , incrementalSceneReuse_(incrementalSceneReuse) {
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

bool RenderContext::incrementalSceneReuseEnabled() const noexcept {
  return incrementalSceneReuse_;
}

void RenderContext::beginCapture(std::vector<NodeId>* out) {
  captureStack_.push_back(CaptureSink{.storage = out, .push = out ? &pushCapturedVector : nullptr});
  if (out) {
    out->clear();
  }
}

void RenderContext::beginCapture(detail::SmallVector<NodeId, 2>* out) {
  captureStack_.push_back(CaptureSink{.storage = out, .push = out ? &pushCapturedSmallVector : nullptr});
  if (out) {
    out->clear();
  }
}

void RenderContext::endCapture() {
#ifndef NDEBUG
  assert(!captureStack_.empty());
#endif
  captureStack_.pop_back();
}

void RenderContext::recordCaptured(NodeId id) {
  if (!captureStack_.empty()) {
    CaptureSink const& sink = captureStack_.back();
    if (sink.storage && sink.push) {
      sink.push(sink.storage, id);
    }
  }
}

NodeId RenderContext::addLayer(NodeId parent, LayerNode node) {
  NodeId const id = graph_.addLayer(parent, std::move(node));
  recordCaptured(id);
  return id;
}

NodeId RenderContext::addRect(NodeId parent, RectNode node) {
  NodeId const id = graph_.addRect(parent, std::move(node));
  recordCaptured(id);
  return id;
}

NodeId RenderContext::addText(NodeId parent, TextNode node) {
  NodeId const id = graph_.addText(parent, std::move(node));
  recordCaptured(id);
  return id;
}

NodeId RenderContext::addImage(NodeId parent, ImageNode node) {
  NodeId const id = graph_.addImage(parent, std::move(node));
  recordCaptured(id);
  return id;
}

NodeId RenderContext::addPath(NodeId parent, PathNode node) {
  NodeId const id = graph_.addPath(parent, std::move(node));
  recordCaptured(id);
  return id;
}

NodeId RenderContext::addLine(NodeId parent, LineNode node) {
  NodeId const id = graph_.addLine(parent, std::move(node));
  recordCaptured(id);
  return id;
}

NodeId RenderContext::addCustomRender(NodeId parent, CustomRenderNode node) {
  NodeId const id = graph_.addCustomRender(parent, std::move(node));
  recordCaptured(id);
  return id;
}

} // namespace flux
