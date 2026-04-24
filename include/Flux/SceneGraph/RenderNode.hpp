#pragma once

/// \file Flux/SceneGraph/RenderNode.hpp
///
/// Scene-graph custom draw node backed by a Canvas callback.

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>

#include <functional>
#include <memory>

namespace flux::scenegraph {

class RenderNode final : public SceneNode {
  public:
    using DrawFunction = std::function<void(Canvas&, Rect)>;

    explicit RenderNode(Rect bounds = {}, DrawFunction draw = {}, bool pure = false);
    ~RenderNode() override;

    DrawFunction const& draw() const noexcept;
    bool pure() const noexcept;

    void setDraw(DrawFunction draw);
    void setPure(bool pure);

    void render(Renderer& renderer) const override;
    bool canPrepareRenderOps() const noexcept override;

  private:
    DrawFunction draw_{};
    bool pure_ = false;
};

} // namespace flux::scenegraph
