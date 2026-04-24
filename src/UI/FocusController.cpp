#include <Flux/UI/FocusController.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/SceneGraph/SceneInteraction.hpp>
#include <Flux/UI/StateStore.hpp>

#include "Debug/PerfCounters.hpp"

#include <cstddef>
namespace flux {

namespace {

void scheduleReactiveDirtyFallback() {
  if (Application::hasInstance()) {
    Application::instance().markReactiveDirty();
  }
}

} // namespace

bool FocusController::hasKeyboardOrigin() const noexcept {
  return lastInputKind_ == FocusInputKind::Keyboard;
}

ComponentKey const& FocusController::focusedKey() const noexcept {
  return focusedKey_;
}

std::optional<OverlayId> FocusController::focusInOverlay() const noexcept {
  return focusInOverlay_;
}

void FocusController::setDirtyMarker(DirtyMarker marker) {
  dirtyMarker_ = marker;
}

bool FocusController::isInSubtree(ComponentKey const& key, StateStore const& store) const noexcept {
  if (focusedKey_.empty() || key.size() > focusedKey_.size()) {
    return false;
  }
  debug::perf::recordComponentKeyPrefixCompare(key.size());
  if (!focusedKey_.hasPrefix(key)) {
    return false;
  }
  std::optional<std::uint64_t> const os = store.overlayScope();
  if (os.has_value()) {
    return focusInOverlay_.has_value() && focusInOverlay_->value == *os;
  }
  return !focusInOverlay_.has_value();
}

bool FocusController::markDirty(ComponentKey const& key, std::optional<OverlayId> overlayScope) const {
  if (key.empty() || !dirtyMarker_) {
    return false;
  }
  return dirtyMarker_(key, overlayScope);
}

void FocusController::markStateTransition(ComponentKey const& previousKey,
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

void FocusController::set(ComponentKey const& key, std::optional<OverlayId> overlayScope, FocusInputKind kind) {
  if (focusedKey_ == key && focusInOverlay_ == overlayScope) {
    return;
  }
  ComponentKey const previousKey = focusedKey_;
  std::optional<OverlayId> const previousOverlayScope = focusInOverlay_;
  focusedKey_ = key;
  focusInOverlay_ = overlayScope;
  lastInputKind_ = kind;
  markStateTransition(previousKey, previousOverlayScope, focusedKey_, focusInOverlay_);
}

void FocusController::clear() {
  if (focusedKey_.empty() && !focusInOverlay_.has_value()) {
    return;
  }
  ComponentKey const previousKey = focusedKey_;
  std::optional<OverlayId> const previousOverlayScope = focusInOverlay_;
  focusedKey_.clear();
  focusInOverlay_.reset();
  markStateTransition(previousKey, previousOverlayScope, focusedKey_, focusInOverlay_);
}

void FocusController::cycleInTree(scenegraph::SceneGraph const& graph, bool reverse,
                                  std::optional<OverlayId> overlayId) {
  std::vector<ComponentKey> const order = scenegraph::collectFocusableKeys(graph);
  if (order.empty()) {
    return;
  }

  if (focusedKey_.empty()) {
    set(reverse ? order.back() : order.front(), overlayId, FocusInputKind::Keyboard);
    return;
  }

  auto it = std::find(order.begin(), order.end(), focusedKey_);
  if (it == order.end()) {
    set(reverse ? order.back() : order.front(), overlayId, FocusInputKind::Keyboard);
    return;
  }

  if (!reverse) {
    ++it;
    set(it == order.end() ? order.front() : *it, overlayId, FocusInputKind::Keyboard);
  } else {
    set(it == order.begin() ? order.back() : *std::prev(it), overlayId, FocusInputKind::Keyboard);
  }
}

void FocusController::cycleNonModal(std::vector<OverlayEntry const*> const& overlayEntries,
                                    scenegraph::SceneGraph const& mainGraph, bool reverse) {
  std::vector<std::pair<ComponentKey, std::optional<OverlayId>>> merged;
  for (OverlayEntry const* p : overlayEntries) {
    OverlayEntry const& e = *p;
    for (ComponentKey const& k : scenegraph::collectFocusableKeys(e.sceneGraph)) {
      merged.push_back({k, e.id});
    }
  }
  for (ComponentKey const& k : scenegraph::collectFocusableKeys(mainGraph)) {
    merged.push_back({k, std::nullopt});
  }
  if (merged.empty()) {
    return;
  }

  std::size_t idx = 0;
  bool found = false;
  for (std::size_t i = 0; i < merged.size(); ++i) {
    if (merged[i].first == focusedKey_ && focusInOverlay_ == merged[i].second) {
      idx = i;
      found = true;
      break;
    }
  }

  if (!found || focusedKey_.empty()) {
    if (reverse) {
      set(merged.back().first, merged.back().second, FocusInputKind::Keyboard);
    } else {
      set(merged.front().first, merged.front().second, FocusInputKind::Keyboard);
    }
    return;
  }

  if (!reverse) {
    std::size_t const next = (idx + 1) % merged.size();
    set(merged[next].first, merged[next].second, FocusInputKind::Keyboard);
  } else {
    std::size_t const prev = idx == 0 ? merged.size() - 1 : idx - 1;
    set(merged[prev].first, merged[prev].second, FocusInputKind::Keyboard);
  }
}

void FocusController::requestInSubtree(ComponentKey const& subtreeKey,
                                       scenegraph::SceneGraph const& graph,
                                       std::optional<OverlayId> overlayId) {
  if (subtreeKey.empty()) {
    return;
  }
  for (ComponentKey const& leafKey : scenegraph::collectFocusableKeys(graph)) {
    if (leafKey.hasPrefix(subtreeKey)) {
      set(leafKey, overlayId, FocusInputKind::Keyboard);
      return;
    }
  }
}

void FocusController::claimFocusForSubtree(ComponentKey const& pressedKey,
                                           scenegraph::SceneGraph const& graph,
                                           std::optional<OverlayId> overlayScope) {
  if (pressedKey.empty()) {
    return;
  }
  std::size_t const parentLen = pressedKey.size() - 1;
  ComponentKey const parentKey = pressedKey.prefix(parentLen == 0 ? 1 : parentLen);
  for (ComponentKey const& leafKey : scenegraph::collectFocusableKeys(graph)) {
    if (leafKey.size() <= parentLen) {
      continue;
    }
    if (leafKey.hasPrefix(parentKey)) {
      set(leafKey, overlayScope, FocusInputKind::Pointer);
      return;
    }
  }
}

void FocusController::onOverlayPushed(OverlayEntry& entry) {
  if (!entry.config.modal) {
    return;
  }
  ComponentKey const previousKey = focusedKey_;
  std::optional<OverlayId> const previousOverlayScope = focusInOverlay_;
  entry.preFocusKey = focusedKey_;
  focusInOverlay_ = entry.id;
  focusedKey_.clear();
  markStateTransition(previousKey, previousOverlayScope, focusedKey_, focusInOverlay_);
}

void FocusController::onOverlayRemoved(OverlayEntry const& entry,
                                       scenegraph::SceneGraph const* mainGraph) {
  if (!mainGraph) {
    if (focusInOverlay_ == entry.id) {
      focusInOverlay_.reset();
    }
    return;
  }
  if (entry.config.modal) {
    if (focusInOverlay_ == entry.id) {
      ComponentKey const previousKey = focusedKey_;
      std::optional<OverlayId> const previousOverlayScope = focusInOverlay_;
      focusInOverlay_.reset();
      focusedKey_ = entry.preFocusKey;
      lastInputKind_ = FocusInputKind::Keyboard;
      if (!focusedKey_.empty()) {
        auto const [node, interaction] = scenegraph::findInteractionByKey(*mainGraph, focusedKey_);
        (void)node;
        if (!interaction || !interaction->focusable) {
          focusedKey_.clear();
        }
      }
      markStateTransition(previousKey, previousOverlayScope, focusedKey_, focusInOverlay_);
    }
  }
}

void FocusController::syncAfterOverlayRebuild(OverlayEntry& entry) {
  if (!entry.config.modal) {
    return;
  }
  if (focusInOverlay_ != entry.id) {
    return;
  }
  std::vector<ComponentKey> const order = scenegraph::collectFocusableKeys(entry.sceneGraph);
  if (order.empty()) {
    return;
  }
  auto const [node, interaction] = scenegraph::findInteractionByKey(entry.sceneGraph, focusedKey_);
  (void)node;
  if (!interaction || !interaction->focusable) {
    ComponentKey const previousKey = focusedKey_;
    std::optional<OverlayId> const previousOverlayScope = focusInOverlay_;
    focusedKey_ = order.front();
    lastInputKind_ = FocusInputKind::Keyboard;
    markStateTransition(previousKey, previousOverlayScope, focusedKey_, focusInOverlay_);
  }
}

void FocusController::validateAfterRebuild(scenegraph::SceneGraph const& mainGraph) {
  if (focusInOverlay_.has_value()) {
    return;
  }
  if (focusedKey_.empty()) {
    return;
  }
  auto const [node, interaction] = scenegraph::findInteractionByKey(mainGraph, focusedKey_);
  (void)node;
  if (!interaction || !interaction->focusable) {
    ComponentKey const previousKey = focusedKey_;
    std::optional<OverlayId> const previousOverlayScope = focusInOverlay_;
    focusedKey_.clear();
    markStateTransition(previousKey, previousOverlayScope, focusedKey_, focusInOverlay_);
  }
}

} // namespace flux
