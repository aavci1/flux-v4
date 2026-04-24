#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/Renderer.hpp>

#include <algorithm>
#include <utility>

namespace flux::scenegraph {

namespace {

Rect expandForStrokeAndShadow(Rect rect, StrokeStyle const &stroke, ShadowStyle const &shadow) noexcept {
    float const strokeInset = stroke.isNone() ? 0.f : stroke.width * 0.5f;
    float const blur = shadow.isNone() ? 0.f : shadow.radius;
    float const left = std::max(strokeInset, blur - shadow.offset.x);
    float const right = std::max(strokeInset, blur + shadow.offset.x);
    float const top = std::max(strokeInset, blur - shadow.offset.y);
    float const bottom = std::max(strokeInset, blur + shadow.offset.y);
    rect.x -= left;
    rect.y -= top;
    rect.width += left + right;
    rect.height += top + bottom;
    return rect;
}

} // namespace

struct RectNode::Impl {
    FillStyle fill = FillStyle::none();
    StrokeStyle stroke = StrokeStyle::none();
    CornerRadius cornerRadius {};
    ShadowStyle shadow = ShadowStyle::none();
    bool clipsContents = false;
    float opacity = 1.f;
};

RectNode::RectNode(Rect bounds, FillStyle fill, StrokeStyle stroke, CornerRadius cornerRadius, ShadowStyle shadow) : SceneNode(SceneNodeKind::Rect, bounds), impl_(std::make_unique<Impl>()) {
    impl_->fill = std::move(fill);
    impl_->stroke = std::move(stroke);
    impl_->cornerRadius = cornerRadius;
    impl_->shadow = shadow;
}

RectNode::~RectNode() = default;

FillStyle const &RectNode::fill() const noexcept {
    return impl_->fill;
}

StrokeStyle const &RectNode::stroke() const noexcept {
    return impl_->stroke;
}

CornerRadius RectNode::cornerRadius() const noexcept {
    return impl_->cornerRadius;
}

ShadowStyle const &RectNode::shadow() const noexcept {
    return impl_->shadow;
}

bool RectNode::clipsContents() const noexcept {
    return impl_->clipsContents;
}

float RectNode::opacity() const noexcept {
    return impl_->opacity;
}

void RectNode::setFill(FillStyle fill) {
    if (impl_->fill == fill) {
        return;
    }
    impl_->fill = std::move(fill);
    markDirty();
}

void RectNode::setStroke(StrokeStyle stroke) {
    if (impl_->stroke == stroke) {
        return;
    }
    impl_->stroke = std::move(stroke);
    markDirty();
}

void RectNode::setCornerRadius(CornerRadius cornerRadius) {
    if (impl_->cornerRadius == cornerRadius) {
        return;
    }
    impl_->cornerRadius = cornerRadius;
    markDirty();
}

void RectNode::setShadow(ShadowStyle shadow) {
    if (impl_->shadow == shadow) {
        return;
    }
    impl_->shadow = shadow;
    markDirty();
}

void RectNode::setClipsContents(bool clipsContents) noexcept {
    impl_->clipsContents = clipsContents;
}

void RectNode::setOpacity(float opacity) noexcept {
    impl_->opacity = std::clamp(opacity, 0.f, 1.f);
}

Rect RectNode::localBounds() const noexcept {
    return expandForStrokeAndShadow(Rect::sharp(0.f, 0.f, size().width, size().height),
                                    impl_->stroke, impl_->shadow);
}

void RectNode::render(Renderer &renderer) const {
    renderer.drawRect(Rect::sharp(0.f, 0.f, size().width, size().height), impl_->cornerRadius,
                      impl_->fill, impl_->stroke, impl_->shadow);
}

} // namespace flux::scenegraph
