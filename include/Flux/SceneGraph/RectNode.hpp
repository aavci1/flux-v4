#pragma once

/// \file Flux/SceneGraph/RectNode.hpp
///
/// Scene-graph rectangle node.

#include <Flux/Graphics/Styles.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>

namespace flux::scenegraph {

class RectNode final : public SceneNode {
  public:
    explicit RectNode(Rect bounds = {}, FillStyle fill = FillStyle::none(), StrokeStyle stroke = StrokeStyle::none(), CornerRadius cornerRadius = {}, ShadowStyle shadow = ShadowStyle::none());
    ~RectNode() override;

    FillStyle const &fill() const noexcept;
    StrokeStyle const &stroke() const noexcept;
    CornerRadius cornerRadius() const noexcept;
    ShadowStyle const &shadow() const noexcept;
    bool clipsContents() const noexcept;
    float opacity() const noexcept;

    void setFill(FillStyle fill);
    void setStroke(StrokeStyle stroke);
    void setCornerRadius(CornerRadius cornerRadius);
    void setShadow(ShadowStyle shadow);
    void setClipsContents(bool clipsContents) noexcept;
    void setOpacity(float opacity) noexcept;

    void render(Renderer &renderer) const override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace flux::scenegraph
