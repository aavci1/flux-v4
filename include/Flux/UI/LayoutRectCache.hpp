#pragma once

#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/ComponentKey.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/Core/Types.hpp>

#include <optional>
#include <unordered_map>

namespace flux {

class StateStore;

/// Two-generation layout rect cache. Filled at the end of each rebuild pass.
/// Components read their own rect via `forCurrentComponent()`; overlays and
/// popovers read anchor rects via `forLeafKeyPrefix()`.
class LayoutRectCache {
public:
  /// Called at end of `BuildOrchestrator::rebuild()`. Swaps generations and
  /// fills `current_` from `ctx.subtreeRootLayers()`.
  void fill(SceneGraph const& graph, BuildContext const& ctx);

  /// Rect for the composite currently executing `body()`.
  /// Returns nullopt when called outside a build pass.
  std::optional<Rect> forCurrentComponent(StateStore const& store) const;

  /// Rect for a specific composite key. Used by `useLayoutRect()` when given
  /// a captured key from a prior `body()` call.
  std::optional<Rect> forKey(ComponentKey const& key) const;

  /// Longest-prefix rect for a leaf `stableTargetKey`. Used by popover anchor
  /// tracking and `forTapAnchor`.
  std::optional<Rect> forLeafKeyPrefix(ComponentKey const& stableTargetKey) const;

  /// During `onTap`: rect for the innermost composite ancestor of the tapped
  /// node. `tapLeafKey` must be the leaf `stableTargetKey` set before `onTap`
  /// fires; empty otherwise.
  std::optional<Rect> forTapAnchor(ComponentKey const& tapLeafKey) const;

private:
  std::unordered_map<ComponentKey, Rect, ComponentKeyHash> current_{};
  std::unordered_map<ComponentKey, Rect, ComponentKeyHash> prev_{};
};

} // namespace flux
