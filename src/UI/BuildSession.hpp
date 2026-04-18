#pragma once

#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Scene/NodeId.hpp>
#include <Flux/Scene/SceneNode.hpp>
#include <Flux/UI/Environment.hpp>

#include <memory>
#include <optional>

namespace flux {

class SceneBuilder;
class SceneGeometryIndex;
class TextSystem;

ResolvedRootScene resolveOverlayRootScene(std::optional<Element> const& content,
                                          ComponentKey rootKey = ComponentKey{LocalId::fromIndex(0)});

class BuildSession {
public:
  BuildSession(TextSystem& textSystem, EnvironmentStack& environment,
               EnvironmentLayer windowEnvironment, SceneGeometryIndex* geometryIndex);

  std::unique_ptr<SceneNode> buildRoot(ResolvedRootScene const& resolved, NodeId id,
                                       LayoutConstraints const& constraints,
                                       std::unique_ptr<SceneNode> existing = nullptr) const;

private:
  TextSystem& textSystem_;
  EnvironmentStack& environment_;
  EnvironmentLayer windowEnvironment_;
  SceneGeometryIndex* geometryIndex_ = nullptr;
};

} // namespace flux
