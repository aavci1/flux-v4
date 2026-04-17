#pragma once

/// \file Flux/Scene/RenderSceneNode.hpp
///
/// Part of the Flux public API.

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Scene/SceneNode.hpp>

#include <functional>

namespace flux {

class RenderSceneNode final : public SceneNode {
public:
  explicit RenderSceneNode(NodeId id);

  std::function<void(Canvas&, Rect)> draw{};
  Rect frame{};
  bool pure = false;

  bool paints() const noexcept override { return static_cast<bool>(draw); }
  void rebuildLocalPaint() override;
  Rect computeOwnBounds() const override;
};

} // namespace flux
