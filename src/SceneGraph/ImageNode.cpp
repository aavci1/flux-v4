#include <Flux/SceneGraph/ImageNode.hpp>
#include <Flux/SceneGraph/Renderer.hpp>

#include <utility>

namespace flux::scenegraph {

struct ImageNode::Impl {
    std::shared_ptr<Image const> image;
};

ImageNode::ImageNode(Rect bounds, std::shared_ptr<Image const> image)
    : SceneNode(SceneNodeKind::Image, bounds), impl_(std::make_unique<Impl>()) {
    impl_->image = std::move(image);
}

ImageNode::~ImageNode() = default;

std::shared_ptr<Image const> const &ImageNode::image() const noexcept {
    return impl_->image;
}

void ImageNode::setImage(std::shared_ptr<Image const> image) {
    if (impl_->image == image) {
        return;
    }
    impl_->image = std::move(image);
    markDirty();
}

void ImageNode::render(Renderer &renderer) const {
    if (!impl_->image) {
        return;
    }
    renderer.drawImage(*impl_->image, localBounds());
}

} // namespace flux::scenegraph
