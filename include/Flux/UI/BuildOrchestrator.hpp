#pragma once

#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Reactive/Observer.hpp>
#include <Flux/UI/ActionRegistry.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/LayoutRectCache.hpp>
#include <Flux/UI/StateStore.hpp>

#include <functional>
#include <memory>
#include <optional>

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
  BuildOrchestrator(Window& window, FocusController& focus, HoverController& hover, GestureTracker& gesture,
                    Runtime* runtime);

  ~BuildOrchestrator();

  void setRoot(std::unique_ptr<RootHolder> holder);
  /// Registers the reactive frame callback; typically `[&] { runtime.rebuild(...); }` so `sCurrent` is set.
  void subscribeToRebuild(std::function<void()> onFrameNeeded);

  void rebuild(std::optional<Size> sizeOverride = std::nullopt);

  StateStore& stateStore() noexcept { return stateStore_; }
  LayoutEngine& layoutEngine() noexcept { return layoutEngine_; }
  LayoutRectCache& layoutRects() noexcept { return layoutRects_; }
  LayoutRectCache const& layoutRects() const noexcept { return layoutRects_; }
  EventMap const& mainEventMap() const noexcept { return eventMap_; }
  ActionRegistry& actionRegistryForBuild() noexcept { return actionRegistryBuild_; }
  ActionRegistry const& actionRegistryCommitted() const noexcept { return actionRegistryCommitted_; }

  Rect buildSlotRect() const { return layoutEngine_.childFrame(); }

private:
  Window& window_;
  FocusController& focus_;
  HoverController& hover_;
  GestureTracker& gesture_;
  Runtime* runtime_{};

  std::unique_ptr<RootHolder> rootHolder_;
  LayoutEngine layoutEngine_;
  StateStore stateStore_;
  LayoutRectCache layoutRects_;
  EventMap eventMap_;
  ObserverHandle rebuildHandle_{};
  ActionRegistry actionRegistryBuild_{};
  ActionRegistry actionRegistryCommitted_{};
};

} // namespace flux
