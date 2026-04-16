#include <Flux/UI/BuildOrchestrator.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/LayoutTree.hpp>
#include <Flux/UI/RenderContext.hpp>
#include <Flux/UI/RenderLayoutTree.hpp>
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

bool constraintsEqual(LayoutConstraints const& a, LayoutConstraints const& b) {
  return a.minWidth == b.minWidth && a.minHeight == b.minHeight &&
         a.maxWidth == b.maxWidth && a.maxHeight == b.maxHeight;
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
  latestLayoutIsCurrent_ = false;
  latestRootIdentityToken_ = 0;
  layoutTree_.clear();
  retainedLayoutTree_.clear();
  layoutSubtreeRoots_.clear();
  retainedSubtreeRoots_.clear();
  layoutPins_.reset();
  retainedLayoutPins_.reset();
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

  bool const canReuseWholeLayout =
      rootHolder_ && latestLayoutIsCurrent_ && !stateStore_.hasPendingDirtyComponents() &&
      latestRootIdentityToken_ == rootHolder_->layoutIdentityToken() &&
      constraintsEqual(latestRootConstraints_, rootCs);

  SceneGraph& graph = window_.sceneGraph();
  if (canReuseWholeLayout) {
    window_.overlayManager().rebuild(sz, runtime);
    return;
  }

  graph.clear();

  layoutEngine_.resetForBuild();
  ++textFrameIndex_;
  Application::instance().textSystem().onFrameBegin(textFrameIndex_);
  layoutDebugBeginPass();

  EventMap newMap;

  stateStore_.beginRebuild(sizeOverride.has_value() || !stateStore_.hasPendingDirtyComponents());
  if (stateStore_.shouldForceFullRebuild()) {
    measureCache_.clear();
  }

  actionRegistryBuild_.beginRebuild();
  StateStore::setCurrent(&stateStore_);

  if (latestLayoutIsCurrent_) {
    retainedLayoutTree_.clear();
    std::swap(retainedLayoutTree_, layoutTree_);
    retainedSubtreeRoots_ = std::move(layoutSubtreeRoots_);
    retainedLayoutPins_ = std::move(layoutPins_);
    latestLayoutIsCurrent_ = false;
  }
  layoutTree_.clear();
  LayoutContext lctx{Application::instance().textSystem(), layoutEngine_, layoutTree_, &measureCache_,
                     &retainedLayoutTree_, &retainedSubtreeRoots_};
  lctx.pushConstraints(rootCs);
  EnvironmentLayer windowEnvBaseline = window_.environmentLayer();
  EnvironmentStack::current().push(std::move(windowEnvBaseline));
  layoutEngine_.setChildFrame(Rect{0.f, 0.f, sz.width, sz.height});
  if (rootHolder_) {
    rootHolder_->layoutInto(lctx);
  }
  EnvironmentStack::current().pop();
  lctx.popConstraints();

  RenderContext rctx{graph, newMap, Application::instance().textSystem()};
  rctx.pushConstraints(rootCs);
  renderLayoutTree(layoutTree_, rctx);
  rctx.popConstraints();

  layoutRects_.fill(layoutTree_, lctx);
  layoutSubtreeRoots_ = lctx.subtreeRootLayouts();
  layoutPins_ = lctx.pinnedElements();
  latestRootConstraints_ = rootCs;
  latestRootIdentityToken_ = rootHolder_ ? rootHolder_->layoutIdentityToken() : 0;
  latestLayoutIsCurrent_ = true;
  layoutDebugEndPass();

  StateStore::setCurrent(nullptr);
  stateStore_.endRebuild();

  eventMap_ = std::move(newMap);
  focus_.validateAfterRebuild(eventMap_);

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

LayoutEngine& BuildOrchestrator::layoutEngine() noexcept {
  return layoutEngine_;
}

LayoutRectCache& BuildOrchestrator::layoutRects() noexcept {
  return layoutRects_;
}

LayoutRectCache const& BuildOrchestrator::layoutRects() const noexcept {
  return layoutRects_;
}

LayoutTree const& BuildOrchestrator::layoutTree() const noexcept {
  return layoutTree_;
}

EventMap const& BuildOrchestrator::mainEventMap() const noexcept {
  return eventMap_;
}

ActionRegistry& BuildOrchestrator::actionRegistryForBuild() noexcept {
  return actionRegistryBuild_;
}

ActionRegistry const& BuildOrchestrator::actionRegistryCommitted() const noexcept {
  return actionRegistryCommitted_;
}

Rect BuildOrchestrator::buildSlotRect() const {
  return layoutEngine_.lastAssignedFrame();
}

} // namespace flux
