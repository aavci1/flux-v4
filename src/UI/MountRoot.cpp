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

struct EnvironmentScope {
  EnvironmentStack& stack;
  bool pushed = false;

  EnvironmentScope(EnvironmentStack& environment, EnvironmentLayer layer)
      : stack(environment) {
    stack.push(std::move(layer));
    pushed = true;
  }

  ~EnvironmentScope() {
    if (pushed) {
      stack.pop();
    }
  }
};

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
                     EnvironmentLayer environment, Size viewportSize,
                     std::function<void()> requestRedraw)
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
  layoutDebugBeginPass();
  if (mounted_) {
    unmount(sceneGraph);
  }

  MeasureContext measureContext{textSystem_};
  EnvironmentStack& environment = EnvironmentStack::current();
  EnvironmentScope const environmentScope{environment, environment_};
  MountContext context{rootScope_, environment, textSystem_, measureContext,
                       rootConstraints(viewportSize_), LayoutHints{}, requestRedraw_};

  auto node = Reactive::withOwner(rootScope_, [&] {
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
  rootScope_ = Reactive::Scope{};
  mounted_ = false;
}

void MountRoot::resize(Size viewportSize, scenegraph::SceneGraph& sceneGraph) {
  viewportSize_ = viewportSize;
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
