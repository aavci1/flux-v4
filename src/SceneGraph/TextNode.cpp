#include <Flux/SceneGraph/Renderer.hpp>
#include <Flux/SceneGraph/TextNode.hpp>

#include <utility>

namespace flux::scenegraph {

struct TextNode::Impl {
    std::shared_ptr<TextLayout const> layout;
};

TextNode::TextNode(Rect bounds, std::shared_ptr<TextLayout const> layout)
    : SceneNode(SceneNodeKind::Text, bounds), impl_(std::make_unique<Impl>()) {
    impl_->layout = std::move(layout);
}

TextNode::~TextNode() = default;

std::shared_ptr<TextLayout const> const &TextNode::layout() const noexcept {
    return impl_->layout;
}

void TextNode::setLayout(std::shared_ptr<TextLayout const> layout) {
    if (impl_->layout == layout) {
        return;
    }
    impl_->layout = std::move(layout);
    markDirty();
}

void TextNode::render(Renderer &renderer) const {
    if (!impl_->layout) {
        return;
    }
    renderer.drawTextLayout(*impl_->layout);
}

} // namespace flux::scenegraph
