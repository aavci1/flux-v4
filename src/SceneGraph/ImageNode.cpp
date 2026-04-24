#include <Flux/SceneGraph/ImageNode.hpp>
#include <Flux/SceneGraph/Renderer.hpp>

#include <utility>

namespace flux::scenegraph {

struct ImageNode::Impl {
    std::shared_ptr<Image const> image;
    ImageFillMode fillMode = ImageFillMode::Cover;
};

ImageNode::ImageNode(Rect bounds, std::shared_ptr<Image const> image, ImageFillMode fillMode)
    : SceneNode(SceneNodeKind::Image, bounds), impl_(std::make_unique<Impl>()) {
    impl_->image = std::move(image);
    impl_->fillMode = fillMode;
}

ImageNode::~ImageNode() = default;

std::shared_ptr<Image const> const &ImageNode::image() const noexcept {
    return impl_->image;
}

ImageFillMode ImageNode::fillMode() const noexcept {
    return impl_->fillMode;
}

void ImageNode::setImage(std::shared_ptr<Image const> image) {
    if (impl_->image == image) {
        return;
    }
    impl_->image = std::move(image);
    markDirty();
}

void ImageNode::setFillMode(ImageFillMode fillMode) {
    if (impl_->fillMode == fillMode) {
        return;
    }
    impl_->fillMode = fillMode;
    markDirty();
}

void ImageNode::render(Renderer &renderer) const {
    if (!impl_->image) {
        return;
    }
    renderer.drawImage(*impl_->image, localBounds(), impl_->fillMode);
}

} // namespace flux::scenegraph
