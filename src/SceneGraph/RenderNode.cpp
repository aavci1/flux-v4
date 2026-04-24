#include <Flux/SceneGraph/RenderNode.hpp>

#include <Flux/SceneGraph/Renderer.hpp>

#include <utility>

namespace flux::scenegraph {

struct RenderNode::Impl {
    DrawFunction draw {};
    bool pure = false;
};

RenderNode::RenderNode(Rect bounds, DrawFunction draw, bool pure)
    : SceneNode(SceneNodeKind::Render, bounds), impl_(std::make_unique<Impl>()) {
    impl_->draw = std::move(draw);
    impl_->pure = pure;
}

RenderNode::~RenderNode() = default;

RenderNode::DrawFunction const &RenderNode::draw() const noexcept {
    return impl_->draw;
}

bool RenderNode::pure() const noexcept {
    return impl_->pure;
}

void RenderNode::setDraw(DrawFunction drawValue) {
    impl_->draw = std::move(drawValue);
    markDirty();
}

void RenderNode::setPure(bool pureValue) {
    if (impl_->pure == pureValue) {
        return;
    }
    impl_->pure = pureValue;
    markDirty();
}

void RenderNode::render(Renderer &renderer) const {
    Canvas *canvas = renderer.canvas();
    if (!canvas || !impl_->draw) {
        return;
    }
    impl_->draw(*canvas, localBounds());
}

bool RenderNode::canPrepareRenderOps() const noexcept {
    return impl_->pure;
}

} // namespace flux::scenegraph
