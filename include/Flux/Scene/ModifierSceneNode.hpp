#pragma once

/// \file Flux/Scene/ModifierSceneNode.hpp
///
/// Part of the Flux public API.

#include <Flux/Graphics/Styles.hpp>
#include <Flux/Scene/SceneNode.hpp>

#include <optional>

namespace flux {

class ModifierSceneNode final : public SceneNode {
public:
  explicit ModifierSceneNode(NodeId id);

  Rect chromeRect{};
  std::optional<Rect> clip{};
  float opacity = 1.f;
  BlendMode blendMode = BlendMode::Normal;
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
  ShadowStyle shadow = ShadowStyle::none();
  CornerRadius cornerRadius{};

  bool paints() const noexcept override;
  void applyNodeState(Renderer&) const override;
  void rebuildLocalPaint() override;
  Rect computeOwnBounds() const override;
  Rect adjustSubtreeBounds(Rect r) const override;
  SceneNode* hitTest(Point local) override;
  SceneNode const* hitTest(Point local) const override;
};

} // namespace flux
