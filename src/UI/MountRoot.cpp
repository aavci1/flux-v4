#include <Flux/UI/MountRoot.hpp>

#include <Flux/UI/Detail/RootHolder.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/UI/Detail/LayoutDebugDump.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/MountContext.hpp>

#include "Detail/ResizeTrace.hpp"

#include <chrono>
#include <utility>

namespace flux {

namespace {

LayoutConstraints rootConstraints(Size size) {
  return LayoutConstraints{
      .maxWidth = size.width,
      .maxHeight = size.height,
      .minWidth = size.width,
      .minHeight = size.height,
  };
}

} // namespace

MountRoot::MountRoot(std::unique_ptr<RootHolder> root, TextSystem& textSystem,
                     EnvironmentBinding environment, Size viewportSize,
                     Reactive::SmallFn<void()> requestRedraw)
    : root_(std::move(root))
    , textSystem_(textSystem)
    , environment_(std::move(environment))
    , viewportSize_(viewportSize)
    , requestRedraw_(std::move(requestRedraw)) {}

MountRoot::~MountRoot() = default;

void MountRoot::mount(scenegraph::SceneGraph& sceneGraph) {
  if (!root_) {
    return;
  }
  layoutDebugAttachSceneGraph(&sceneGraph);
  layoutDebugBeginPass();
  if (mounted_) {
    unmount(sceneGraph);
  }

  MeasureContext measureContext{textSystem_, environment_};
  MountContext context{rootScope_, textSystem_, measureContext,
                       rootConstraints(viewportSize_), LayoutHints{}, requestRedraw_,
                       environment_};

  auto node = Reactive::withOwner(rootScope_, [&] {
    detail::CurrentMountContextScope const currentMountContext{context};
    detail::HookLayoutScope const hookScope{rootConstraints(viewportSize_)};
    Element rootElement = root_->makeElement();
    detail::HookInteractionSignalScope const interactionScope{rootScope_};
    return rootElement.mount(context);
  });
  if (node) {
    sceneGraph.setRoot(std::move(node));
    layoutDebugDumpRetained(sceneGraph);
    mounted_ = true;
  }
  layoutDebugEndPass();
}

void MountRoot::unmount(scenegraph::SceneGraph& sceneGraph) {
  rootScope_.dispose();
  sceneGraph.releaseRoot();
  layoutDebugAttachSceneGraph(&sceneGraph);
  rootScope_ = Reactive::Scope{};
  mounted_ = false;
}

void MountRoot::resize(Size viewportSize, scenegraph::SceneGraph& sceneGraph) {
  bool const traceResize = detail::resizeTraceEnabled();
  auto const resizeStart = traceResize ? std::chrono::steady_clock::now()
                                       : std::chrono::steady_clock::time_point{};
  viewportSize_ = viewportSize;
  layoutDebugAttachSceneGraph(&sceneGraph);
  if (!mounted_) {
    mount(sceneGraph);
    if (traceResize) {
      auto const elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - resizeStart).count();
      detail::resizeTrace("mount-root",
                          "resize-mounted size=%.0fx%.0f elapsed=%.3fms\n",
                          viewportSize_.width,
                          viewportSize_.height,
                          static_cast<double>(elapsed) / 1000.0);
    }
    return;
  }
  layoutDebugBeginPass();
  auto const relayoutStart = traceResize ? std::chrono::steady_clock::now()
                                         : std::chrono::steady_clock::time_point{};
  if (!sceneGraph.root().relayout(rootConstraints(viewportSize_))) {
    sceneGraph.root().setSize(viewportSize_);
  }
  std::int64_t relayoutElapsed = 0;
  if (traceResize) {
    relayoutElapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - relayoutStart).count();
  }
  layoutDebugDumpRetained(sceneGraph);
  layoutDebugEndPass();
  if (traceResize) {
    auto const elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - resizeStart).count();
    detail::resizeTrace("mount-root",
                        "resize size=%.0fx%.0f relayout=%.3fms elapsed=%.3fms\n",
                        viewportSize_.width,
                        viewportSize_.height,
                        static_cast<double>(relayoutElapsed) / 1000.0,
                        static_cast<double>(elapsed) / 1000.0);
  }
}

} // namespace flux
