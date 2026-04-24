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

RectNode::RectNode(Rect bounds, FillStyle fill, StrokeStyle stroke, CornerRadius cornerRadius,
                   ShadowStyle shadow)
    : SceneNode(SceneNodeKind::Rect, bounds)
    , fill_(std::move(fill))
    , stroke_(std::move(stroke))
    , cornerRadius_(cornerRadius)
    , shadow_(shadow) {}

RectNode::~RectNode() = default;

FillStyle const &RectNode::fill() const noexcept {
    return fill_;
}

StrokeStyle const &RectNode::stroke() const noexcept {
    return stroke_;
}

CornerRadius RectNode::cornerRadius() const noexcept {
    return cornerRadius_;
}

ShadowStyle const &RectNode::shadow() const noexcept {
    return shadow_;
}

bool RectNode::clipsContents() const noexcept {
    return clipsContents_;
}

float RectNode::opacity() const noexcept {
    return opacity_;
}

void RectNode::setFill(FillStyle fill) {
    if (fill_ == fill) {
        return;
    }
    fill_ = std::move(fill);
    markDirty();
}

void RectNode::setStroke(StrokeStyle stroke) {
    if (stroke_ == stroke) {
        return;
    }
    stroke_ = std::move(stroke);
    markDirty();
}

void RectNode::setCornerRadius(CornerRadius cornerRadius) {
    if (cornerRadius_ == cornerRadius) {
        return;
    }
    cornerRadius_ = cornerRadius;
    markDirty();
}

void RectNode::setShadow(ShadowStyle shadow) {
    if (shadow_ == shadow) {
        return;
    }
    shadow_ = shadow;
    markDirty();
}

void RectNode::setClipsContents(bool clipsContents) noexcept {
    clipsContents_ = clipsContents;
}

void RectNode::setOpacity(float opacity) noexcept {
    opacity_ = std::clamp(opacity, 0.f, 1.f);
}

Rect RectNode::localBounds() const noexcept {
    return expandForStrokeAndShadow(Rect::sharp(0.f, 0.f, size().width, size().height),
                                    stroke_, shadow_);
}

void RectNode::render(Renderer &renderer) const {
    renderer.drawRect(Rect::sharp(0.f, 0.f, size().width, size().height), cornerRadius_,
                      fill_, stroke_, shadow_);
}

} // namespace flux::scenegraph
