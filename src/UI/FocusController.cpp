#include <Flux/UI/FocusController.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Scene/SceneTreeInteraction.hpp>
#include <Flux/UI/StateStore.hpp>

#include <algorithm>
#include <cstddef>
namespace flux {

bool FocusController::hasKeyboardOrigin() const noexcept {
  return lastInputKind_ == FocusInputKind::Keyboard;
}

ComponentKey const& FocusController::focusedKey() const noexcept {
  return focusedKey_;
}

std::optional<OverlayId> FocusController::focusInOverlay() const noexcept {
  return focusInOverlay_;
}

bool FocusController::isInSubtree(ComponentKey const& key, StateStore const& store) const noexcept {
  if (focusedKey_.empty() || key.size() > focusedKey_.size()) {
    return false;
  }
  if (!std::equal(key.begin(), key.end(), focusedKey_.begin())) {
    return false;
  }
  std::optional<std::uint64_t> const os = store.overlayScope();
  if (os.has_value()) {
    return focusInOverlay_.has_value() && focusInOverlay_->value == *os;
  }
  return !focusInOverlay_.has_value();
}

void FocusController::set(ComponentKey const& key, std::optional<OverlayId> overlayScope, FocusInputKind kind) {
  if (focusedKey_ == key && focusInOverlay_ == overlayScope) {
    return;
  }
  focusedKey_ = key;
  focusInOverlay_ = overlayScope;
  lastInputKind_ = kind;
  Application::instance().markReactiveDirty();
}

void FocusController::clear() {
  if (focusedKey_.empty() && !focusInOverlay_.has_value()) {
    return;
  }
  focusedKey_.clear();
  focusInOverlay_.reset();
  Application::instance().markReactiveDirty();
}

void FocusController::cycleInTree(SceneTree const& tree, bool reverse, std::optional<OverlayId> overlayId) {
  std::vector<ComponentKey> const order = collectFocusableKeys(tree);
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
                                    SceneTree const& mainTree, bool reverse) {
  std::vector<std::pair<ComponentKey, std::optional<OverlayId>>> merged;
  for (OverlayEntry const* p : overlayEntries) {
    OverlayEntry const& e = *p;
    for (ComponentKey const& k : collectFocusableKeys(e.sceneTree)) {
      merged.push_back({k, e.id});
    }
  }
  for (ComponentKey const& k : collectFocusableKeys(mainTree)) {
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

void FocusController::requestInSubtree(ComponentKey const& subtreeKey, SceneTree const& tree,
                                       std::optional<OverlayId> overlayId) {
  if (subtreeKey.empty()) {
    return;
  }
  for (ComponentKey const& leafKey : collectFocusableKeys(tree)) {
    if (leafKey.size() >= subtreeKey.size() &&
        std::equal(subtreeKey.begin(), subtreeKey.end(), leafKey.begin())) {
      set(leafKey, overlayId, FocusInputKind::Keyboard);
      return;
    }
  }
}

void FocusController::claimFocusForSubtree(ComponentKey const& pressedKey, SceneTree const& tree,
                                           std::optional<OverlayId> overlayScope) {
  if (pressedKey.empty()) {
    return;
  }
  std::size_t const parentLen = pressedKey.size() - 1;
  for (ComponentKey const& leafKey : collectFocusableKeys(tree)) {
    if (leafKey.size() <= parentLen) {
      continue;
    }
    bool sameParent = false;
    if (parentLen == 0) {
      std::size_t const len = std::min(leafKey.size(), pressedKey.size());
      sameParent = len > 0 &&
                   std::equal(pressedKey.begin(), pressedKey.begin() + static_cast<std::ptrdiff_t>(len),
                              leafKey.begin());
    } else {
      sameParent = std::equal(pressedKey.begin(),
                              pressedKey.begin() + static_cast<std::ptrdiff_t>(parentLen), leafKey.begin());
    }
    if (sameParent) {
      set(leafKey, overlayScope, FocusInputKind::Pointer);
      return;
    }
  }
}

void FocusController::onOverlayPushed(OverlayEntry& entry) {
  if (!entry.config.modal) {
    return;
  }
  entry.preFocusKey = focusedKey_;
  focusInOverlay_ = entry.id;
  focusedKey_.clear();
}

void FocusController::onOverlayRemoved(OverlayEntry const& entry, SceneTree const* mainTree) {
  if (!mainTree) {
    if (focusInOverlay_ == entry.id) {
      focusInOverlay_.reset();
    }
    return;
  }
  if (entry.config.modal) {
    if (focusInOverlay_ == entry.id) {
      focusInOverlay_.reset();
      focusedKey_ = entry.preFocusKey;
      lastInputKind_ = FocusInputKind::Keyboard;
      if (!focusedKey_.empty()) {
        auto const [id, interaction] = findInteractionByKey(*mainTree, focusedKey_);
        (void)id;
        if (!interaction || !interaction->focusable) {
          focusedKey_.clear();
        }
      }
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
  std::vector<ComponentKey> const order = collectFocusableKeys(entry.sceneTree);
  if (order.empty()) {
    return;
  }
  auto const [id, interaction] = findInteractionByKey(entry.sceneTree, focusedKey_);
  (void)id;
  if (!interaction || !interaction->focusable) {
    focusedKey_ = order.front();
    lastInputKind_ = FocusInputKind::Keyboard;
  }
}

void FocusController::validateAfterRebuild(SceneTree const& mainTree) {
  if (focusInOverlay_.has_value()) {
    return;
  }
  if (focusedKey_.empty()) {
    return;
  }
  auto const [id, interaction] = findInteractionByKey(mainTree, focusedKey_);
  (void)id;
  if (!interaction || !interaction->focusable) {
    focusedKey_.clear();
  }
}

} // namespace flux
