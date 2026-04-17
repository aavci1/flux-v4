#include <Flux/Scene/TextSceneNode.hpp>

namespace flux {

TextSceneNode::TextSceneNode(NodeId id)
    : SceneNode(SceneNodeKind::Text, id) {}

void TextSceneNode::rebuildLocalPaint() {
  SceneNode::rebuildLocalPaint();
  if (!layout && textSystem) {
    TextLayoutOptions opts{};
    opts.horizontalAlignment = horizontalAlignment;
    opts.verticalAlignment = verticalAlignment;
    opts.wrapping = wrapping;
    opts.maxLines = maxLines;
    opts.firstBaselineOffset = firstBaselineOffset;
    if (allocation.width > 0.f || allocation.height > 0.f) {
      layout = textSystem->layout(text, font, color, allocation, opts);
    } else {
      layout = textSystem->layout(text, font, color, widthConstraint, opts);
    }
  }
  if (layout) {
    localPaintCache().push_back(DrawTextPaintCommand{
        .layout = layout,
        .origin = allocation.width > 0.f || allocation.height > 0.f ? Point{allocation.x, allocation.y} : origin,
    });
  }
}

Rect TextSceneNode::computeOwnBounds() const {
  if (allocation.width > 0.f || allocation.height > 0.f) {
    return allocation;
  }
  if (layout) {
    return Rect{origin.x, origin.y, layout->measuredSize.width, layout->measuredSize.height};
  }
  return {};
}

} // namespace flux
