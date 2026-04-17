#pragma once

/// \file Flux/Scene/LineSceneNode.hpp
///
/// Part of the Flux public API.

#include <Flux/Graphics/Styles.hpp>
#include <Flux/Scene/SceneNode.hpp>

namespace flux {

class LineSceneNode final : public SceneNode {
public:
  explicit LineSceneNode(NodeId id);

  Point from{};
  Point to{};
  StrokeStyle stroke = StrokeStyle::none();

  bool paints() const noexcept override { return true; }
  void rebuildLocalPaint() override;
  Rect computeOwnBounds() const override;
};

} // namespace flux
