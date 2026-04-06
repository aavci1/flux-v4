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
#include <Flux/UI/MeasureCache.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/LayoutTree.hpp>

#include <Flux/Scene/NodeId.hpp>
#include <Flux/UI/Environment.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace flux {

class Element;
class FocusController;
class HoverController;
class GestureTracker;
class LayoutContext;
class LayoutTree;
class Runtime;
class Window;

struct CompositeSnapshot {
  LayoutNodeId layoutRoot{};
  NodeId sceneLayerId{};
  LayoutConstraints constraints{};
  LayoutHints hints{};
  Size measuredSize{};
  Element const* element = nullptr;
  std::vector<EnvironmentLayer> ancestorEnv;
};

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
  EventMap const& mainEventMap() const noexcept;
  ActionRegistry& actionRegistryForBuild() noexcept;
  ActionRegistry const& actionRegistryCommitted() const noexcept;

  Rect buildSlotRect() const;

private:
  void buildSnapshots(LayoutTree const& tree, LayoutContext const& ctx,
                      std::unordered_map<ComponentKey, NodeId, ComponentKeyHash> const& sceneParents);
  bool tryPartialRebuild(Runtime& runtime);

  Window& window_;
  FocusController& focus_;
  HoverController& hover_;
  GestureTracker& gesture_;

  std::unique_ptr<RootHolder> rootHolder_;
  LayoutEngine layoutEngine_;
  StateStore stateStore_;
  LayoutRectCache layoutRects_;
  EventMap eventMap_;
  ObserverHandle rebuildHandle_{};
  ActionRegistry actionRegistryBuild_{};
  ActionRegistry actionRegistryCommitted_{};
  MeasureCache measureCache_{};

  std::unordered_map<ComponentKey, CompositeSnapshot, ComponentKeyHash> snapshots_{};
};

} // namespace flux
