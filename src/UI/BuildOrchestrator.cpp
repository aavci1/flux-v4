#include <Flux/UI/BuildOrchestrator.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/SceneTree.hpp>
#include <Flux/UI/SceneBuilder.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/Overlay.hpp>

#include <Flux/UI/Detail/LayoutDebugDump.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace flux {

namespace {

Size snapRootLayoutSize(Size s) {
  return { std::max(1.f, std::round(s.width)), std::max(1.f, std::round(s.height)) };
}

bool envTruthy(char const* v) {
  return v && v[0] != '\0' && std::strcmp(v, "0") != 0 && std::strcmp(v, "false") != 0;
}

bool inputDebugEnabled() {
  static int cached = -1;
  if (cached < 0) {
    cached = envTruthy(std::getenv("FLUX_DEBUG_INPUT")) ? 1 : 0;
  }
  return cached != 0;
}

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

  stateStore_.beginRebuild(sizeOverride.has_value() || !stateStore_.hasPendingDirtyComponents());
  actionRegistryBuild_.beginRebuild();
  StateStore::setCurrent(&stateStore_);
  buildSlotRect_ = Rect{0.f, 0.f, sz.width, sz.height};

  {
    SceneTree& sceneTree = window_.sceneTree();
    std::unique_ptr<SceneNode> existingRoot = sceneTree.takeRoot();
    Element const* sceneRoot = nullptr;
    bool rootDescendantsStable = false;
    if (rootHolder_) {
      rootHolder_->prepareSceneElement(rootCs);
      sceneRoot = rootHolder_->sceneElementForCurrentBuild();
      rootDescendantsStable = rootHolder_->sceneElementDescendantsStable();
    }
    if (sceneRoot) {
      SceneBuilder sceneBuilder{Application::instance().textSystem(), EnvironmentStack::current(), &sceneGeometry_};
      EnvironmentLayer windowEnv = window_.environmentLayer();
      EnvironmentStack::current().push(std::move(windowEnv));
      ComponentKey const rootKey = rootHolder_->sceneRootKey();
      if (StateStore* store = StateStore::current()) {
        store->pushCompositePathStable(rootDescendantsStable);
      }
      std::unique_ptr<SceneNode> nextRoot =
          sceneBuilder.build(*sceneRoot, NodeId{1ull}, rootCs, std::move(existingRoot), rootKey);
      if (StateStore* store = StateStore::current()) {
        store->popCompositePathStable();
      }
      EnvironmentStack::current().pop();
      sceneTree.setRoot(std::move(nextRoot));
      layoutDebugDumpRetained(sceneTree, sceneGeometry_);
    } else {
      sceneGeometry_.clear();
      sceneTree.clear();
      layoutDebugDumpRetained(sceneTree, sceneGeometry_);
    }
  }
  layoutDebugEndPass();

  StateStore::setCurrent(nullptr);
  stateStore_.endRebuild();

  focus_.validateAfterRebuild(window_.sceneTree());

  window_.overlayManager().rebuild(sz, runtime);

  std::swap(actionRegistryBuild_, actionRegistryCommitted_);

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

SceneGeometryIndex& BuildOrchestrator::sceneGeometry() noexcept {
  return sceneGeometry_;
}

SceneGeometryIndex const& BuildOrchestrator::sceneGeometry() const noexcept {
  return sceneGeometry_;
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
