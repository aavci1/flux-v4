#include <Flux/UI/BuildOrchestrator.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/Overlay.hpp>

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
  // is set for hooks (`Runtime::current()`) during the build pass.
}

void BuildOrchestrator::subscribeToRebuild(std::function<void()> onFrameNeeded) {
  rebuildHandle_ = Application::instance().onNextFrameNeeded(std::move(onFrameNeeded));
}

void BuildOrchestrator::rebuild(std::optional<Size> sizeOverride, Runtime& runtime) {
  if (sizeOverride.has_value()) {
    gesture_.clearPress();
  }

  SceneGraph& graph = window_.sceneGraph();
  graph.clear();

  layoutEngine_.resetForBuild();

  actionRegistryBuild_.beginRebuild();

  stateStore_.beginRebuild();
  StateStore::setCurrent(&stateStore_);

  EventMap newMap;
  BuildContext ctx{ graph, newMap, Application::instance().textSystem(), layoutEngine_ };
  Size const raw = sizeOverride.value_or(window_.getSize());
  Size const sz = snapRootLayoutSize(raw);
  LayoutConstraints rootCs{};
  rootCs.maxWidth = sz.width;
  rootCs.maxHeight = sz.height;
  ctx.pushConstraints(rootCs);
  EnvironmentLayer windowEnvBaseline = window_.environmentLayer();
  EnvironmentStack::current().push(std::move(windowEnvBaseline));
  if (rootHolder_) {
    rootHolder_->buildInto(ctx);
  }
  EnvironmentStack::current().pop();
  ctx.popConstraints();

  layoutRects_.fill(graph, ctx);

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
  window_.requestRedraw();
}

} // namespace flux
