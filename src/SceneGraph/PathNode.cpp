#include <Flux/SceneGraph/PathNode.hpp>
#include <Flux/SceneGraph/Renderer.hpp>

#include <utility>

namespace flux::scenegraph {

struct PathNode::Impl {
    Path path{};
    FillStyle fill = FillStyle::none();
    StrokeStyle stroke = StrokeStyle::none();
    ShadowStyle shadow = ShadowStyle::none();
};

PathNode::PathNode(Rect bounds, Path path, FillStyle fill, StrokeStyle stroke, ShadowStyle shadow)
    : SceneNode(SceneNodeKind::Path, bounds), impl_(std::make_unique<Impl>()) {
    impl_->path = std::move(path);
    impl_->fill = std::move(fill);
    impl_->stroke = std::move(stroke);
    impl_->shadow = std::move(shadow);
}

PathNode::~PathNode() = default;

Path const& PathNode::path() const noexcept {
    return impl_->path;
}

FillStyle const& PathNode::fill() const noexcept {
    return impl_->fill;
}

StrokeStyle const& PathNode::stroke() const noexcept {
    return impl_->stroke;
}

ShadowStyle const& PathNode::shadow() const noexcept {
    return impl_->shadow;
}

void PathNode::setPath(Path pathValue) {
    if (impl_->path.contentHash() == pathValue.contentHash()) {
        return;
    }
    impl_->path = std::move(pathValue);
    markDirty();
}

void PathNode::setFill(FillStyle fillValue) {
    if (impl_->fill == fillValue) {
        return;
    }
    impl_->fill = std::move(fillValue);
    markDirty();
}

void PathNode::setStroke(StrokeStyle strokeValue) {
    if (impl_->stroke == strokeValue) {
        return;
    }
    impl_->stroke = std::move(strokeValue);
    markDirty();
}

void PathNode::setShadow(ShadowStyle shadowValue) {
    if (impl_->shadow == shadowValue) {
        return;
    }
    impl_->shadow = std::move(shadowValue);
    markDirty();
}

Rect PathNode::localBounds() const noexcept {
    return impl_->path.getBounds();
}

void PathNode::render(Renderer& renderer) const {
    renderer.drawPath(impl_->path, impl_->fill, impl_->stroke, impl_->shadow);
}

} // namespace flux::scenegraph
