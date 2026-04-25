#include "UI/Build/BuildPass.hpp"

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/SceneBuilder.hpp>

namespace flux {

namespace {

struct BuildPassScope {
  BuildPassConfig const& config;
  StateStore* previousStore = StateStore::current();

  explicit BuildPassScope(BuildPassConfig const& config)
      : config(config) {
    config.stateStore.beginRebuild(config.forceFullRebuild);
    config.stateStore.setOverlayScope(config.overlayScope);
    StateStore::setCurrent(&config.stateStore);
  }

  ~BuildPassScope() {
    StateStore::setCurrent(previousStore);
    config.stateStore.setOverlayScope(std::nullopt);
    config.stateStore.endRebuild();
  }
};

struct WindowEnvironmentScope {
  EnvironmentStack& environment;

  WindowEnvironmentScope(EnvironmentStack& environment, EnvironmentLayer const& windowEnvironment)
      : environment(environment) {
    environment.push(windowEnvironment);
  }

  ~WindowEnvironmentScope() {
    environment.pop();
  }
};

std::unique_ptr<scenegraph::SceneNode>
buildResolvedRoot(BuildPassConfig const& config, ResolvedRootScene const& resolved,
                  LayoutConstraints const& constraints,
                  std::unique_ptr<scenegraph::SceneNode> existingRoot) {
  if (!resolved.element) {
    if (config.sceneGraph) {
      config.sceneGraph->clearGeometry();
    }
    return nullptr;
  }

  SceneBuilder sceneBuilder {config.textSystem, config.environment, config.sceneGraph};
  WindowEnvironmentScope environmentScope{config.environment, config.windowEnvironment};
  return sceneBuilder.build(*resolved.element, constraints, resolved.rootKey,
                            resolved.rootUsesMaxWidthAsAssigned,
                            resolved.rootUsesMaxHeightAsAssigned,
                            std::move(existingRoot));
}

} // namespace

std::unique_ptr<scenegraph::SceneNode>
runBuildPass(BuildPassConfig const& config, ResolvedRootScene const& resolved,
             LayoutConstraints const& constraints,
             std::unique_ptr<scenegraph::SceneNode> existingRoot) {
  BuildPassScope scope{config};
  return buildResolvedRoot(config, resolved, constraints, std::move(existingRoot));
}

std::unique_ptr<scenegraph::SceneNode>
runBuildPass(BuildPassConfig const& config, std::function<ResolvedRootScene()> resolveRoot,
             LayoutConstraints const& constraints,
             std::unique_ptr<scenegraph::SceneNode> existingRoot) {
  BuildPassScope scope{config};
  ResolvedRootScene const resolved = resolveRoot ? resolveRoot() : ResolvedRootScene{};
  return buildResolvedRoot(config, resolved, constraints, std::move(existingRoot));
}

} // namespace flux
