#include <Flux/UI/BuildOrchestrator.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/UI/SceneBuilder.hpp>

#include <Flux/UI/Detail/LayoutDebugDump.hpp>

#include "UI/Build/BuildPass.hpp"
#include "UI/DebugFlags.hpp"

#include <cmath>
#include <cstdio>
#include <utility>

namespace flux {

namespace {

Size snapRootLayoutSize(Size s) {
  return { std::max(1.f, std::round(s.width)), std::max(1.f, std::round(s.height)) };
}

Point retainedSubtreeRootOffset(scenegraph::SceneGraph const& sceneGraph, StateStore const& store,
                                ComponentKey const& key, ComponentBuildSnapshot const& snapshot) {
  if (scenegraph::SceneNode* existingNode = sceneGraph.nodeForKey(key)) {
    Point retainedOffset = existingNode->position();
    if (std::optional<Rect> const currentRect = sceneGraph.rectForKey(key)) {
      retainedOffset.x -= currentRect->x - snapshot.origin.x;
      retainedOffset.y -= currentRect->y - snapshot.origin.y;
    }
    return retainedOffset;
  }

  if (!key.empty()) {
    ComponentKey parentKey = key;
    parentKey.pop_back();
    if (std::optional<ComponentBuildSnapshot> const parentSnapshot = store.buildSnapshot(parentKey)) {
      return Point{
          snapshot.origin.x - parentSnapshot->origin.x,
          snapshot.origin.y - parentSnapshot->origin.y,
      };
    }
  }

  return {};
}

bool inputDebugEnabled() {
  return debug::inputEnabled();
}

struct WindowEnvironmentScope {
  EnvironmentStack& environment;
  bool active = false;

  WindowEnvironmentScope(EnvironmentStack& environment, EnvironmentLayer const& windowEnvironment)
      : environment(environment), active(true) {
    environment.push(windowEnvironment);
  }

  ~WindowEnvironmentScope() {
    if (active) {
      environment.pop();
    }
  }
};

} // namespace

BuildOrchestrator::BuildOrchestrator(Window& window, FocusController& focus, HoverController& hover,
                                   GestureTracker& gesture)
    : window_(window)
    , focus_(focus)
    , hover_(hover)
    , gesture_(gesture) {
  (void)hover_;
}

BuildOrchestrator::~BuildOrchestrator() {
  if (rebuildHandle_.isValid()) {
    Application::instance().unobserveNextFrame(rebuildHandle_);
  }
  stateStore_.shutdown();
}

void BuildOrchestrator::setRoot(std::unique_ptr<RootHolder> holder) {
  rootHolder_ = std::move(holder);
  // Do not call `rebuild()` here — `Runtime::setRoot` calls `Runtime::rebuild()` so `sCurrent`
  // is set for hooks (`Runtime::current()`) during the layout/render pass.
}

void BuildOrchestrator::subscribeToRebuild(std::function<void()> onFrameNeeded) {
  rebuildHandle_ = Application::instance().onNextFrameNeeded(std::move(onFrameNeeded));
}

bool BuildOrchestrator::tryIncrementalComponentRebuild(LayoutConstraints const& rootConstraints) {
  (void)rootConstraints;
  if (!rootHolder_) {
    return false;
  }

  scenegraph::SceneGraph& sceneGraph = window_.sceneGraph();
  struct IncrementalCandidate {
    ComponentKey key{};
    ComponentBuildSnapshot snapshot{};
    Element const* sceneElement = nullptr;
  };

  std::vector<IncrementalCandidate> candidates{};
  for (ComponentKey const& dirtyKey : stateStore_.pendingDirtyComponents()) {
    std::optional<ComponentBuildSnapshot> const snapshot = stateStore_.buildSnapshot(dirtyKey);
    Element const* const sceneElement = stateStore_.sceneElement(dirtyKey);
    if (!snapshot.has_value() || !sceneElement || !sceneGraph.nodeForKey(dirtyKey)) {
      continue;
    }
    candidates.push_back(IncrementalCandidate{
        .key = dirtyKey,
        .snapshot = *snapshot,
        .sceneElement = sceneElement,
    });
  }

  if (candidates.size() != 1) {
    return false;
  }

  IncrementalCandidate const& candidate = candidates.front();
  ComponentKey const& dirtyKey = candidate.key;
  ComponentBuildSnapshot const& snapshot = candidate.snapshot;
  Element const& sceneElement = *candidate.sceneElement;
  Point const retainedRootOffset =
      retainedSubtreeRootOffset(sceneGraph, stateStore_, dirtyKey, snapshot);

  StateStore* previousStore = StateStore::current();
  stateStore_.beginRebuild(false);
  stateStore_.setOverlayScope(std::nullopt);
  StateStore::setCurrent(&stateStore_);

  auto finishRebuild = [&]() {
    stateStore_.markComponentsOutsideSubtreeVisited(dirtyKey);
    StateStore::setCurrent(previousStore);
    stateStore_.setOverlayScope(std::nullopt);
    stateStore_.endRebuild();
  };

  bool success = false;
  {
    EnvironmentStack& environment = EnvironmentStack::current();
    WindowEnvironmentScope environmentScope{environment, window_.environmentLayer()};
    scenegraph::SceneGraph patchGraph;
    SceneBuilder sceneBuilder{Application::instance().textSystem(), environment, &patchGraph};
    std::unique_ptr<scenegraph::SceneNode> existingSubtree =
        sceneGraph.replaceNodeForKey(dirtyKey, std::make_unique<scenegraph::GroupNode>());
    std::unique_ptr<scenegraph::SceneNode> replacement =
        sceneBuilder.buildSubtree(sceneElement, snapshot.constraints, snapshot.hints, snapshot.origin,
                                  dirtyKey, snapshot.assignedSize, snapshot.hasAssignedWidth,
                                  snapshot.hasAssignedHeight, retainedRootOffset,
                                  std::move(existingSubtree));
    if (replacement) {
      std::optional<Rect> const previousRect = sceneGraph.rectForKey(dirtyKey);
      std::optional<Rect> const nextRect = patchGraph.rectForKey(dirtyKey);
      std::unique_ptr<scenegraph::SceneNode> removed =
          sceneGraph.replaceNodeForKey(dirtyKey, std::move(replacement));
      if (removed) {
        sceneGraph.replaceSubtreeData(dirtyKey, patchGraph);
        if (previousRect && nextRect &&
            (std::abs(previousRect->width - nextRect->width) > 0.001f ||
             std::abs(previousRect->height - nextRect->height) > 0.001f)) {
          Application::instance().markReactiveDirty();
        }
        layoutDebugDumpRetained(sceneGraph);
        success = true;
      }
    }
  }

  finishRebuild();
  return success;
}

void BuildOrchestrator::rebuild(std::optional<Size> sizeOverride, Runtime& runtime) {
  if (sizeOverride.has_value()) {
    gesture_.clearPress();
  }

  Size const raw = sizeOverride.value_or(window_.getSize());
  Size const sz = snapRootLayoutSize(raw);
  LayoutConstraints rootCs{};
  // Match a full-window proposal: min == max so the root subtree always expands to the
  // drawable client area (padding/modifiers shrink the inner min/max together).
  rootCs.minWidth = sz.width;
  rootCs.minHeight = sz.height;
  rootCs.maxWidth = sz.width;
  rootCs.maxHeight = sz.height;

  ++textFrameIndex_;
  Application::instance().textSystem().onFrameBegin(textFrameIndex_);
  layoutDebugBeginPass();
  buildSlotRect_ = Rect{0.f, 0.f, sz.width, sz.height};

  bool const usedIncremental = !sizeOverride.has_value() && tryIncrementalComponentRebuild(rootCs);
  if (!usedIncremental) {
    actionRegistryBuild_.beginRebuild();
    {
      scenegraph::SceneGraph& sceneGraph = window_.sceneGraph();
      std::unique_ptr<scenegraph::SceneNode> nextRoot = runBuildPass(
          BuildPassConfig{
              .stateStore = stateStore_,
              .forceFullRebuild = sizeOverride.has_value() || !stateStore_.hasPendingDirtyComponents(),
              .textSystem = Application::instance().textSystem(),
              .environment = EnvironmentStack::current(),
              .windowEnvironment = window_.environmentLayer(),
              .sceneGraph = &sceneGraph,
          },
          [&]() { return rootHolder_ ? rootHolder_->resolveScene(rootCs) : ResolvedRootScene{}; }, rootCs);
      if (nextRoot) {
        sceneGraph.setRoot(std::move(nextRoot));
        layoutDebugDumpRetained(sceneGraph);
      } else {
        sceneGraph.clearGeometry();
        sceneGraph.setRoot(std::make_unique<scenegraph::GroupNode>());
        layoutDebugDumpRetained(sceneGraph);
      }
    }
  }
  layoutDebugEndPass();

  focus_.validateAfterRebuild(window_.sceneGraph());

  if (!usedIncremental) {
    window_.overlayManager().rebuild(sz, runtime);
    std::swap(actionRegistryBuild_, actionRegistryCommitted_);
  }

  if (inputDebugEnabled()) {
    std::fprintf(stderr,
                 "[flux:input] rebuild layout=%.1fx%.1f scene root children (if any) updated\n",
                 static_cast<double>(sz.width), static_cast<double>(sz.height));
  }
  Application::instance().textSystem().onFrameEnd();
  window_.requestRedraw();
}

StateStore& BuildOrchestrator::stateStore() noexcept {
  return stateStore_;
}

ActionRegistry& BuildOrchestrator::actionRegistryForBuild() noexcept {
  return actionRegistryBuild_;
}

ActionRegistry const& BuildOrchestrator::actionRegistryCommitted() const noexcept {
  return actionRegistryCommitted_;
}

Rect BuildOrchestrator::buildSlotRect() const {
  return buildSlotRect_;
}

} // namespace flux
