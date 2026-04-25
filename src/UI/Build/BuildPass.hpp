#pragma once

#include <functional>
#include <memory>

#include <Flux/Detail/RootHolder.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/StateStore.hpp>

namespace flux {

class TextSystem;

struct BuildPassConfig {
  StateStore& stateStore;
  std::optional<std::uint64_t> overlayScope{};
  bool forceFullRebuild = true;
  TextSystem& textSystem;
  EnvironmentStack& environment;
  EnvironmentLayer windowEnvironment;
  scenegraph::SceneGraph* sceneGraph = nullptr;
};

std::unique_ptr<scenegraph::SceneNode>
runBuildPass(BuildPassConfig const& config, ResolvedRootScene const& resolved,
             LayoutConstraints const& constraints,
             std::unique_ptr<scenegraph::SceneNode> existingRoot = nullptr);

std::unique_ptr<scenegraph::SceneNode>
runBuildPass(BuildPassConfig const& config, std::function<ResolvedRootScene()> resolveRoot,
             LayoutConstraints const& constraints,
             std::unique_ptr<scenegraph::SceneNode> existingRoot = nullptr);

} // namespace flux
