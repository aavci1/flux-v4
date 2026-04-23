#pragma once

/// \file Flux/SceneGraph/TextNode.hpp
///
/// Scene-graph text node.

#include <Flux/Graphics/TextLayout.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>

#include <memory>

namespace flux::scenegraph {

class TextNode final : public SceneNode {
  public:
    explicit TextNode(Rect bounds = {}, std::shared_ptr<TextLayout const> layout = {});
    ~TextNode() override;

    std::shared_ptr<TextLayout const> const &layout() const noexcept;

    void setLayout(std::shared_ptr<TextLayout const> layout);

    void render(Renderer &renderer) const override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace flux::scenegraph
