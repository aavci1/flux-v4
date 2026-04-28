#include <Flux/UI/MountRoot.hpp>

#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/UI/Detail/LayoutDebugDump.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/MountContext.hpp>

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
  viewportSize_ = viewportSize;
  layoutDebugAttachSceneGraph(&sceneGraph);
  if (!mounted_) {
    mount(sceneGraph);
    return;
  }
  layoutDebugBeginPass();
  if (!sceneGraph.root().relayout(rootConstraints(viewportSize_))) {
    sceneGraph.root().setSize(viewportSize_);
  }
  layoutDebugDumpRetained(sceneGraph);
  layoutDebugEndPass();
}

} // namespace flux
