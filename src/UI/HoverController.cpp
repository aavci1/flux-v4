#include <Flux/UI/HoverController.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Scene/HitTester.hpp>
#include <Flux/UI/StateStore.hpp>

namespace flux {

ComponentKey const& HoverController::hoveredKey() const noexcept {
  return hoveredKey_;
}

std::optional<OverlayId> HoverController::hoverInOverlay() const noexcept {
  return hoverInOverlay_;
}

bool HoverController::isInSubtree(ComponentKey const& key, StateStore const& store) const noexcept {
  if (hoveredKey_.empty() || key.size() > hoveredKey_.size()) {
    return false;
  }
  if (!std::equal(key.begin(), key.end(), hoveredKey_.begin())) {
    return false;
  }
  std::optional<std::uint64_t> const os = store.overlayScope();
  if (os.has_value()) {
    return hoverInOverlay_.has_value() && hoverInOverlay_->value == *os;
  }
  return !hoverInOverlay_.has_value();
}

void HoverController::set(ComponentKey const& key, std::optional<OverlayId> overlayScope) {
  if (hoveredKey_ == key && hoverInOverlay_ == overlayScope) {
    return;
  }
  hoveredKey_ = key;
  hoverInOverlay_ = overlayScope;
  Application::instance().requestRebuild();
}

void HoverController::clear() {
  if (hoveredKey_.empty() && !hoverInOverlay_.has_value()) {
    return;
  }
  hoveredKey_.clear();
  hoverInOverlay_.reset();
  Application::instance().requestRebuild();
}

void HoverController::updateForPoint(Point windowPoint, std::vector<OverlayEntry const*> const& overlayEntries,
                                     SceneGraph const& mainGraph, EventMap const& mainEventMap) {
  // `overlayEntries` is top-to-bottom (front = topmost overlay), matching `entries().rbegin()` order.
  for (OverlayEntry const* p : overlayEntries) {
    OverlayEntry const& oe = *p;
    Point const local{ windowPoint.x - oe.resolvedFrame.x, windowPoint.y - oe.resolvedFrame.y };
    auto const acceptFn = [&oe](NodeId id) -> bool {
      return oe.eventMap.find(id) != nullptr;
    };
    HitTester tester{};
    if (auto hit = tester.hitTest(oe.graph, local, acceptFn)) {
      if (EventHandlers const* h = oe.eventMap.find(hit->nodeId)) {
        set(h->stableTargetKey, oe.id);
        return;
      }
    }
  }

  auto const acceptFn = [&mainEventMap](NodeId id) -> bool {
    return mainEventMap.find(id) != nullptr;
  };

  auto hit = HitTester{}.hitTest(mainGraph, windowPoint, acceptFn);
  if (hit) {
    if (EventHandlers const* h = mainEventMap.find(hit->nodeId)) {
      set(h->stableTargetKey, std::nullopt);
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
