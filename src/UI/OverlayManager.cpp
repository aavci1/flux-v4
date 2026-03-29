#include <Flux/UI/Overlay.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/SceneGraphBounds.hpp>
#include <Flux/UI/BuildContext.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace flux {

OverlayEntry const* OverlayManager::top() const {
  if (overlays_.empty()) {
    return nullptr;
  }
  return overlays_.back().get();
}

LayoutConstraints OverlayManager::resolveConstraints(Size windowSize, OverlayConfig const& config) const {
  LayoutConstraints cs{};
  cs.minWidth = 0.f;
  cs.minHeight = 0.f;
  float maxW = windowSize.width;
  float maxH = windowSize.height;
  if (config.maxSize.has_value()) {
    if (std::isfinite(config.maxSize->width)) {
      maxW = std::min(maxW, config.maxSize->width);
    }
    if (std::isfinite(config.maxSize->height)) {
      maxH = std::min(maxH, config.maxSize->height);
    }
  }
  cs.maxWidth = std::max(0.f, maxW);
  cs.maxHeight = std::max(0.f, maxH);
  return cs;
}

Rect OverlayManager::resolveFrame(Size win, OverlayConfig const& cfg, Size content) const {
  if (!cfg.anchor.has_value()) {
    return {0.f, 0.f, win.width, win.height};
  }
  Rect const& a = *cfg.anchor;
  float x = 0.f;
  float y = 0.f;
  switch (cfg.placement) {
  case OverlayConfig::Placement::Below:
    x = a.x;
    y = a.y + a.height;
    break;
  case OverlayConfig::Placement::Above:
    x = a.x;
    y = a.y - content.height;
    break;
  case OverlayConfig::Placement::End:
    x = a.x + a.width;
    y = a.y;
    break;
  case OverlayConfig::Placement::Start:
    x = a.x - content.width;
    y = a.y;
    break;
  }
  x += cfg.offset.x;
  y += cfg.offset.y;
  x = std::clamp(x, 0.f, std::max(0.f, win.width - content.width));
  y = std::clamp(y, 0.f, std::max(0.f, win.height - content.height));
  return {x, y, content.width, content.height};
}

void OverlayManager::insertModalChrome(OverlayEntry& entry, Size windowSize) {
  SceneGraph& g = entry.graph;
  if (!g.node<LayerNode>(g.root())) {
    return;
  }

  float const w = windowSize.width;
  float const h = windowSize.height;

  RectNode backdrop{};
  backdrop.bounds = Rect{0.f, 0.f, w, h};
  backdrop.fill = FillStyle::solid(entry.config.backdropColor);
  backdrop.stroke = StrokeStyle::none();
  NodeId const backdropId = g.addRect(g.root(), std::move(backdrop));

  RectNode capture{};
  capture.bounds = Rect{0.f, 0.f, w, h};
  capture.fill = FillStyle::none();
  capture.stroke = StrokeStyle::none();
  NodeId const captureId = g.addRect(g.root(), std::move(capture));

  EventHandlers captureHandlers{};
  captureHandlers.stableTargetKey = ComponentKey{};
  captureHandlers.onPointerDown = [](Point) {};
  entry.eventMap.insert(captureId, std::move(captureHandlers));

  // Re-fetch root after addRect: NodeStore::insert may reallocate slots_, invalidating
  // any LayerNode* taken before new nodes were inserted.
  auto* rootLayer = g.node<LayerNode>(g.root());
  if (!rootLayer) {
    return;
  }
  std::vector<NodeId> newOrder;
  newOrder.reserve(rootLayer->children.size());
  newOrder.push_back(backdropId);
  newOrder.push_back(captureId);
  for (NodeId c : rootLayer->children) {
    if (c != backdropId && c != captureId) {
      newOrder.push_back(c);
    }
  }
  g.reorder(g.root(), newOrder);
}

void OverlayManager::rebuild(Size windowSize, Runtime& runtime) {
  for (std::unique_ptr<OverlayEntry>& up : overlays_) {
    OverlayEntry& entry = *up;
    entry.graph.clear();
    entry.eventMap.clear();

    entry.stateStore->beginRebuild();
    entry.stateStore->setOverlayScope(entry.id.value);
    StateStore::setCurrent(entry.stateStore.get());

    layoutEngine_.resetForBuild();
    LayoutConstraints const cs = resolveConstraints(windowSize, entry.config);
    BuildContext ctx{entry.graph, entry.eventMap, Application::instance().textSystem(), layoutEngine_};
    ctx.pushConstraints(cs);
    if (entry.content.has_value()) {
      entry.content->build(ctx);
    }
    ctx.popConstraints();

    StateStore::setCurrent(nullptr);
    entry.stateStore->setOverlayScope(std::nullopt);
    entry.stateStore->endRebuild();

    Size contentSize = measureRootContentSize(entry.graph);
    if (contentSize.width <= 0.f || !std::isfinite(contentSize.width)) {
      contentSize.width = 1.f;
    }
    if (contentSize.height <= 0.f || !std::isfinite(contentSize.height)) {
      contentSize.height = 1.f;
    }

    entry.resolvedFrame = resolveFrame(windowSize, entry.config, contentSize);

    if (entry.config.modal) {
      insertModalChrome(entry, windowSize);
    }

    runtime.syncModalOverlayFocusAfterRebuild(entry);
  }
}

OverlayId OverlayManager::push(Element content, OverlayConfig config, Runtime* runtime) {
  auto entry = std::make_unique<OverlayEntry>();
  entry->id = OverlayId{nextId_++};
  entry->content.emplace(std::move(content));
  entry->config = std::move(config);
  overlays_.push_back(std::move(entry));

  if (runtime) {
    runtime->onOverlayPushed(*overlays_.back());
  }
  Application::instance().markReactiveDirty();
  return overlays_.back()->id;
}

void OverlayManager::remove(OverlayId id, Runtime* runtime) {
  if (!id.isValid()) {
    return;
  }
  for (auto it = overlays_.begin(); it != overlays_.end(); ++it) {
    if ((*it)->id.value == id.value) {
      std::unique_ptr<OverlayEntry> removed = std::move(*it);
      overlays_.erase(it);
      if (runtime) {
        runtime->onOverlayRemoved(*removed);
      }
      if (!runtime || !runtime->imploding()) {
        Application::instance().markReactiveDirty();
      }
      if (removed->config.onDismiss) {
        removed->config.onDismiss();
      }
      return;
    }
  }
}

void OverlayManager::clear(Runtime* runtime, bool invokeDismissCallbacks) {
  while (!overlays_.empty()) {
    OverlayEntry& top = *overlays_.back();
    if (invokeDismissCallbacks && top.config.onDismiss) {
      top.config.onDismiss();
    }
    std::unique_ptr<OverlayEntry> removed = std::move(overlays_.back());
    overlays_.pop_back();
    if (runtime) {
      runtime->onOverlayRemoved(*removed);
    }
  }
  if (!runtime || !runtime->imploding()) {
    Application::instance().markReactiveDirty();
  }
}

} // namespace flux
