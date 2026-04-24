#include <Flux/UI/HoverController.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/SceneGraph/SceneInteraction.hpp>
#include <Flux/UI/StateStore.hpp>

#include "Debug/PerfCounters.hpp"

namespace flux {

namespace {

void scheduleReactiveDirtyFallback() {
  if (Application::hasInstance()) {
    Application::instance().markReactiveDirty();
  }
}

} // namespace

ComponentKey const& HoverController::hoveredKey() const noexcept {
  return hoveredKey_;
}

std::optional<OverlayId> HoverController::hoverInOverlay() const noexcept {
  return hoverInOverlay_;
}

void HoverController::setDirtyMarker(DirtyMarker marker) {
  dirtyMarker_ = marker;
}

bool HoverController::isInSubtree(ComponentKey const& key, StateStore const& store) const noexcept {
  if (hoveredKey_.empty() || key.size() > hoveredKey_.size()) {
    return false;
  }
  debug::perf::recordComponentKeyPrefixCompare(key.size());
  if (!hoveredKey_.hasPrefix(key)) {
    return false;
  }
  std::optional<std::uint64_t> const os = store.overlayScope();
  if (os.has_value()) {
    return hoverInOverlay_.has_value() && hoverInOverlay_->value == *os;
  }
  return !hoverInOverlay_.has_value();
}

bool HoverController::markDirty(ComponentKey const& key, std::optional<OverlayId> overlayScope) const {
  if (key.empty() || !dirtyMarker_) {
    return false;
  }
  return dirtyMarker_(key, overlayScope);
}

void HoverController::markStateTransition(ComponentKey const& previousKey,
                                          std::optional<OverlayId> previousOverlayScope,
                                          ComponentKey const& nextKey,
                                          std::optional<OverlayId> nextOverlayScope) const {
  bool dirty = false;
  dirty |= markDirty(previousKey, previousOverlayScope);
  dirty |= markDirty(nextKey, nextOverlayScope);
  if (!dirty) {
    scheduleReactiveDirtyFallback();
  }
}

void HoverController::set(ComponentKey const& key, std::optional<OverlayId> overlayScope) {
  if (hoveredKey_ == key && hoverInOverlay_ == overlayScope) {
    return;
  }
  ComponentKey const previousKey = hoveredKey_;
  std::optional<OverlayId> const previousOverlayScope = hoverInOverlay_;
  hoveredKey_ = key;
  hoverInOverlay_ = overlayScope;
  markStateTransition(previousKey, previousOverlayScope, hoveredKey_, hoverInOverlay_);
}

void HoverController::clear() {
  if (hoveredKey_.empty() && !hoverInOverlay_.has_value()) {
    return;
  }
  ComponentKey const previousKey = hoveredKey_;
  std::optional<OverlayId> const previousOverlayScope = hoverInOverlay_;
  hoveredKey_.clear();
  hoverInOverlay_.reset();
  markStateTransition(previousKey, previousOverlayScope, hoveredKey_, hoverInOverlay_);
}

void HoverController::updateForPoint(Point windowPoint, std::vector<OverlayEntry const*> const& overlayEntries,
                                     scenegraph::SceneGraph const& mainGraph) {
  // `overlayEntries` is top-to-bottom (front = topmost overlay), matching `entries().rbegin()` order.
  for (OverlayEntry const* p : overlayEntries) {
    OverlayEntry const& oe = *p;
    Point const local{ windowPoint.x - oe.resolvedFrame.x, windowPoint.y - oe.resolvedFrame.y };
    if (auto hit = scenegraph::hitTestInteraction(oe.sceneGraph, local)) {
      if (hit->interaction && !hit->interaction->stableTargetKey.empty()) {
        set(hit->interaction->stableTargetKey, oe.id);
      } else {
        clear();
      }
      return;
    }
  }

  auto hit = scenegraph::hitTestInteraction(mainGraph, windowPoint);
  if (hit) {
    if (hit->interaction && !hit->interaction->stableTargetKey.empty()) {
      set(hit->interaction->stableTargetKey, std::nullopt);
    } else {
      clear();
    }
  } else {
    clear();
  }
}

void HoverController::onOverlayRemoved(OverlayId id, bool teardown) {
  if (hoverInOverlay_ == id) {
    hoverInOverlay_.reset();
    if (!teardown) {
      hoveredKey_.clear();
    }
  }
}

} // namespace flux
