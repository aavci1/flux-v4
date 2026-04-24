#pragma once

/// \file Flux/SceneGraph/ImageNode.hpp
///
/// Scene-graph image node.

#include <Flux/SceneGraph/SceneNode.hpp>
#include <Flux/Graphics/ImageFillMode.hpp>

#include <memory>

namespace flux {
class Image;
}

namespace flux::scenegraph {

class ImageNode final : public SceneNode {
  public:
    explicit ImageNode(Rect bounds = {}, std::shared_ptr<Image const> image = {},
                       ImageFillMode fillMode = ImageFillMode::Cover);
    ~ImageNode() override;

    std::shared_ptr<Image const> const &image() const noexcept;
    ImageFillMode fillMode() const noexcept;

    void setImage(std::shared_ptr<Image const> image);
    void setFillMode(ImageFillMode fillMode);

    void render(Renderer &renderer) const override;

  private:
    std::shared_ptr<Image const> image_{};
    ImageFillMode fillMode_ = ImageFillMode::Cover;
};

} // namespace flux::scenegraph
