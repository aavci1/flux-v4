#include <Flux/UI/Overlay.hpp>

#include <Flux/UI/Element.hpp>

#include <algorithm>
#include <tuple>
#include <utility>

namespace flux {

std::tuple<std::function<void(Element, OverlayConfig)>, std::function<void()>, bool> useOverlay() {
  return {
      [](Element, OverlayConfig) {},
      [] {},
      false,
  };
}

void OverlayManager::rebuild(Size windowSize, Runtime& runtime) {
  (void)windowSize;
  (void)runtime;
}

OverlayId OverlayManager::push(Element content, OverlayConfig config, Runtime* runtime) {
  (void)content;
  (void)runtime;
  auto entry = std::make_unique<OverlayEntry>();
  entry->id = OverlayId{nextId_++};
  entry->config = std::move(config);
  OverlayId id = entry->id;
  overlays_.push_back(std::move(entry));
  return id;
}

void OverlayManager::remove(OverlayId id, Runtime* runtime) {
  (void)runtime;
  std::erase_if(overlays_, [&](std::unique_ptr<OverlayEntry> const& entry) {
    return entry && entry->id == id;
  });
}

void OverlayManager::clear(Runtime* runtime, bool invokeDismissCallbacks) {
  (void)runtime;
  if (invokeDismissCallbacks) {
    for (auto const& entry : overlays_) {
      if (entry && entry->config.onDismiss) {
        entry->config.onDismiss();
      }
    }
  }
  overlays_.clear();
}

OverlayEntry const* OverlayManager::top() const {
  return overlays_.empty() ? nullptr : overlays_.back().get();
}

OverlayEntry* OverlayManager::find(OverlayId id) {
  for (auto& entry : overlays_) {
    if (entry && entry->id == id) {
      return entry.get();
    }
  }
  return nullptr;
}

OverlayEntry const* OverlayManager::find(OverlayId id) const {
  for (auto const& entry : overlays_) {
    if (entry && entry->id == id) {
      return entry.get();
    }
  }
  return nullptr;
}

} // namespace flux
