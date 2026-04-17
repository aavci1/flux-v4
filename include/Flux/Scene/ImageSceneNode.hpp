#pragma once

/// \file Flux/Scene/ImageSceneNode.hpp
///
/// Part of the Flux public API.

#include <Flux/Graphics/Image.hpp>
#include <Flux/Graphics/ImageFillMode.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Scene/SceneNode.hpp>

#include <memory>

namespace flux {

class ImageSceneNode final : public SceneNode {
public:
  explicit ImageSceneNode(NodeId id);

  std::shared_ptr<Image> image{};
  Size size{};
  ImageFillMode fillMode = ImageFillMode::Cover;
  CornerRadius cornerRadius{};
  float opacity = 1.f;

  bool paints() const noexcept override { return true; }
  void rebuildLocalPaint() override;
  Rect computeOwnBounds() const override;
};

} // namespace flux
