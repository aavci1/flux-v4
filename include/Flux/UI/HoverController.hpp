#pragma once

#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/Core/Types.hpp>

#include <optional>
#include <vector>

namespace flux {

class StateStore;

/// Owns geometric hover state for one window.
class HoverController {
public:
  bool isInSubtree(ComponentKey const& key, StateStore const& store) const noexcept;

  ComponentKey const& hoveredKey() const noexcept { return hoveredKey_; }
  std::optional<OverlayId> hoverInOverlay() const noexcept { return hoverInOverlay_; }

  void set(ComponentKey const& key, std::optional<OverlayId> overlayScope);
  void clear();

  void updateForPoint(Point windowPoint, std::vector<OverlayEntry const*> const& overlayEntries,
                      SceneGraph const& mainGraph, EventMap const& mainEventMap);

  /// When `teardown` is true (window destroying), only clears overlay scope — not `hoveredKey_`
  /// (matches legacy `imploding_` path).
  void onOverlayRemoved(OverlayId id, bool teardown = false);

private:
  ComponentKey hoveredKey_{};
  std::optional<OverlayId> hoverInOverlay_{};
};

} // namespace flux
