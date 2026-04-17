#pragma once

/// \file Flux/UI/BuildOrchestrator.hpp
///
/// Part of the Flux public API.


#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Reactive/Observer.hpp>
#include <Flux/UI/ActionRegistry.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/LayoutRectCache.hpp>
#include <Flux/UI/LayoutTree.hpp>
#include <Flux/UI/MeasureCache.hpp>
#include <Flux/UI/SceneGeometryIndex.hpp>
#include <Flux/UI/StateStore.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

namespace flux {

class FocusController;
class HoverController;
class GestureTracker;
class Runtime;
class Window;

/// Drives the reactive rebuild loop for one window.
/// Owns `StateStore`, `LayoutEngine`, `RootHolder`, and `LayoutRectCache`.
class BuildOrchestrator {
public:
  BuildOrchestrator(Window& window, FocusController& focus, HoverController& hover, GestureTracker& gesture);

  ~BuildOrchestrator();

  void setRoot(std::unique_ptr<RootHolder> holder);
  /// Registers the reactive frame callback; typically `[&] { runtime.rebuild(...); }` so `sCurrent` is set.
  void subscribeToRebuild(std::function<void()> onFrameNeeded);

  /// `runtime` is passed in for `OverlayManager::rebuild` only (not stored — avoids a header cycle).
  void rebuild(std::optional<Size> sizeOverride, Runtime& runtime);

  StateStore& stateStore() noexcept;
  LayoutEngine& layoutEngine() noexcept;
  LayoutRectCache& layoutRects() noexcept;
  LayoutRectCache const& layoutRects() const noexcept;
  SceneGeometryIndex& sceneGeometry() noexcept;
  SceneGeometryIndex const& sceneGeometry() const noexcept;
  LayoutTree const& layoutTree() const noexcept;
  EventMap const& mainEventMap() const noexcept;
  ActionRegistry& actionRegistryForBuild() noexcept;
  ActionRegistry const& actionRegistryCommitted() const noexcept;

  Rect buildSlotRect() const;

private:
  Window& window_;
  FocusController& focus_;
  HoverController& hover_;
  GestureTracker& gesture_;

  std::unique_ptr<RootHolder> rootHolder_;
  LayoutEngine layoutEngine_;
  StateStore stateStore_;
  LayoutRectCache layoutRects_;
  LayoutContext::SubtreeRootMap layoutSubtreeRoots_{};
  std::uint64_t layoutSubtreeRootEpoch_{0};
  std::vector<std::shared_ptr<detail::ElementPinStorage>> pinGenerations_{};
  LayoutTree layoutTree_{};
  LayoutConstraints latestRootConstraints_{};
  std::uint64_t latestRootIdentityToken_{0};
  bool latestLayoutIsCurrent_{false};
  EventMap eventMap_;
  ObserverHandle rebuildHandle_{};
  ActionRegistry actionRegistryBuild_{};
  ActionRegistry actionRegistryCommitted_{};
  MeasureCache measureCache_{};
  SceneGeometryIndex sceneGeometry_{};
  std::uint64_t textFrameIndex_{0};
};

} // namespace flux
