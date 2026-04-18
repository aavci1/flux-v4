#include <Flux/Scene/ModifierSceneNode.hpp>

#include <Flux/Scene/Renderer.hpp>

#include "Scene/SceneGeometry.hpp"

namespace flux {

ModifierSceneNode::ModifierSceneNode(NodeId id)
    : SceneNode(SceneNodeKind::Modifier, id) {}

bool ModifierSceneNode::paints() const noexcept {
  return !fill.isNone() || !stroke.isNone() || !shadow.isNone();
}

void ModifierSceneNode::applyNodeState(Renderer& renderer) const {
  if (clip.has_value()) {
    renderer.clipRect(*clip);
  }
  renderer.setOpacity(opacity);
  renderer.setBlendMode(blendMode);
}

void ModifierSceneNode::rebuildLocalPaint() {
  SceneNode::rebuildLocalPaint();
  if (paints() && (chromeRect.width > 0.f || chromeRect.height > 0.f)) {
    localPaintCache().push_back(DrawRectPaintCommand{
        .rect = chromeRect,
        .cornerRadius = cornerRadius,
        .fill = fill,
        .stroke = stroke,
        .shadow = shadow,
    });
  }
  markBoundsDirty();
}

Rect ModifierSceneNode::computeOwnBounds() const {
  if (!paints() || (chromeRect.width <= 0.f && chromeRect.height <= 0.f)) {
    return {};
  }
  return scene::expandForStrokeAndShadow(chromeRect, stroke, shadow);
}

Rect ModifierSceneNode::adjustSubtreeBounds(Rect r) const {
  if (clip.has_value()) {
    return scene::intersectRects(r, *clip);
  }
  return r;
}

SceneNode* ModifierSceneNode::hitTest(Point local) {
  if (clip.has_value() && !clip->contains(local)) {
    return nullptr;
  }
  return SceneNode::hitTest(local);
}

SceneNode const* ModifierSceneNode::hitTest(Point local) const {
  return const_cast<ModifierSceneNode*>(this)->hitTest(local);
}

} // namespace flux
