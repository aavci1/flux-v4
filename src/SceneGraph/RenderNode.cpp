#include <Flux/SceneGraph/RenderNode.hpp>

#include <Flux/SceneGraph/Renderer.hpp>

#include <utility>

namespace flux::scenegraph {

RenderNode::RenderNode(Rect bounds, DrawFunction draw, bool pure)
    : SceneNode(SceneNodeKind::Render, bounds), draw_(std::move(draw)), pure_(pure) {}

RenderNode::~RenderNode() = default;

RenderNode::DrawFunction const &RenderNode::draw() const noexcept {
    return draw_;
}

bool RenderNode::pure() const noexcept {
    return pure_;
}

void RenderNode::setDraw(DrawFunction drawValue) {
    draw_ = std::move(drawValue);
    markDirty();
}

void RenderNode::setPure(bool pureValue) {
    if (pure_ == pureValue) {
        return;
    }
    pure_ = pureValue;
    markDirty();
}

void RenderNode::render(Renderer &renderer) const {
    Canvas *canvas = renderer.canvas();
    if (!canvas || !draw_) {
        return;
    }
    draw_(*canvas, localBounds());
}

bool RenderNode::canPrepareRenderOps() const noexcept {
    return pure_;
}

} // namespace flux::scenegraph
