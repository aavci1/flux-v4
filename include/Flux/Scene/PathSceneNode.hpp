#pragma once

/// \file Flux/Scene/PathSceneNode.hpp
///
/// Part of the Flux public API.

#include <Flux/Graphics/Path.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Scene/SceneNode.hpp>

namespace flux {

class PathSceneNode final : public SceneNode {
public:
  explicit PathSceneNode(NodeId id);

  Path path{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
  ShadowStyle shadow = ShadowStyle::none();

  bool paints() const noexcept override { return true; }
  void rebuildLocalPaint() override;
  Rect computeOwnBounds() const override;
};

} // namespace flux
