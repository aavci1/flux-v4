#pragma once

/// \file Flux/Scene/CustomTransformSceneNode.hpp
///
/// Part of the Flux public API.

#include <Flux/Scene/SceneNode.hpp>

namespace flux {

class CustomTransformSceneNode final : public SceneNode {
public:
  explicit CustomTransformSceneNode(NodeId id);

  Mat3 transform = Mat3::identity();

  void applyNodeState(Renderer&) const override;
  void recomputeBounds() override;
  SceneNode* hitTest(Point local) override;
  SceneNode const* hitTest(Point local) const override;
};

} // namespace flux
