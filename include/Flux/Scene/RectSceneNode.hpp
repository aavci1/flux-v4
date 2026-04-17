#pragma once

/// \file Flux/Scene/RectSceneNode.hpp
///
/// Part of the Flux public API.

#include <Flux/Graphics/Styles.hpp>
#include <Flux/Scene/SceneNode.hpp>

namespace flux {

class RectSceneNode final : public SceneNode {
public:
  explicit RectSceneNode(NodeId id);

  Size size{};
  CornerRadius cornerRadius{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
  ShadowStyle shadow = ShadowStyle::none();

  bool paints() const noexcept override { return true; }
  void rebuildLocalPaint() override;
  Rect computeOwnBounds() const override;
};

} // namespace flux
