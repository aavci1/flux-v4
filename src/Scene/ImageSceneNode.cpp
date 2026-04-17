#include <Flux/Scene/ImageSceneNode.hpp>

namespace flux {

ImageSceneNode::ImageSceneNode(NodeId id)
    : SceneNode(SceneNodeKind::Image, id) {}

void ImageSceneNode::rebuildLocalPaint() {
  SceneNode::rebuildLocalPaint();
  if (image) {
    localPaintCache().push_back(DrawImagePaintCommand{
        .image = image,
        .bounds = Rect{0.f, 0.f, size.width, size.height},
        .fillMode = fillMode,
        .cornerRadius = cornerRadius,
        .opacity = opacity,
    });
  }
}

Rect ImageSceneNode::computeOwnBounds() const {
  return Rect{0.f, 0.f, size.width, size.height};
}

} // namespace flux
