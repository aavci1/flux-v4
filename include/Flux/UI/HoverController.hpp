#pragma once

/// \file Flux/UI/HoverController.hpp
///
/// Part of the Flux public API.


#include <Flux/Scene/SceneTree.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/Core/Types.hpp>

#include <optional>
#include <vector>

namespace flux {

class StateStore;

/// Owns geometric hover state for one window.
class HoverController {
public:
  bool isInSubtree(ComponentKey const& key, StateStore const& store) const noexcept;

  ComponentKey const& hoveredKey() const noexcept;
  std::optional<OverlayId> hoverInOverlay() const noexcept;

  void set(ComponentKey const& key, std::optional<OverlayId> overlayScope);
  void clear();

  void updateForPoint(Point windowPoint, std::vector<OverlayEntry const*> const& overlayEntries,
                      SceneTree const& mainTree);

  /// When `teardown` is true (window destroying), only clears overlay scope — not `hoveredKey_`
  /// (matches legacy `imploding_` path).
  void onOverlayRemoved(OverlayId id, bool teardown = false);

private:
  ComponentKey hoveredKey_{};
  std::optional<OverlayId> hoverInOverlay_{};
};

} // namespace flux
