#include <Flux/SceneGraph/Renderer.hpp>
#include <Flux/SceneGraph/TextNode.hpp>

#include <utility>

namespace flux::scenegraph {

TextNode::TextNode(Rect bounds, std::shared_ptr<TextLayout const> layout)
    : SceneNode(SceneNodeKind::Text, bounds), layout_(std::move(layout)) {}

TextNode::~TextNode() = default;

std::shared_ptr<TextLayout const> const &TextNode::layout() const noexcept {
    return layout_;
}

void TextNode::setLayout(std::shared_ptr<TextLayout const> layout) {
    if (layout_ == layout) {
        return;
    }
    layout_ = std::move(layout);
    markDirty();
}

void TextNode::render(Renderer &renderer) const {
    if (!layout_) {
        return;
    }
    renderer.drawTextLayout(*layout_);
}

} // namespace flux::scenegraph
