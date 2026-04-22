#pragma once

/// \file Flux/SceneGraph/SceneNode.hpp
///
/// Pure scene-graph node types. Each node stores parent-space bounds and renders itself in local coordinates.

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/ImageFillMode.hpp>
#include <Flux/Graphics/Path.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextLayout.hpp>
#include <Flux/SceneGraph/Renderer.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace flux {

class Image;

namespace scenegraph {

enum class SceneNodeKind : std::uint8_t {
    Group,
    Rect,
    Text,
    Line,
    Path,
    Image,
};

std::string_view sceneNodeKindName(SceneNodeKind kind) noexcept;

class SceneNode {
  public:
    explicit SceneNode(SceneNodeKind kind, Rect bounds = {});
    virtual ~SceneNode() = default;

    SceneNode(SceneNode const &) = delete;
    SceneNode &operator=(SceneNode const &) = delete;
    SceneNode(SceneNode &&) = delete;
    SceneNode &operator=(SceneNode &&) = delete;

    SceneNodeKind kind() const noexcept { return kind_; }

    Rect bounds {};

    SceneNode *parent() const noexcept { return parent_; }
    std::span<std::unique_ptr<SceneNode> const> children() const noexcept { return children_; }
    std::span<std::unique_ptr<SceneNode>> children() noexcept { return children_; }

    void appendChild(std::unique_ptr<SceneNode> child);
    void insertChild(std::size_t index, std::unique_ptr<SceneNode> child);
    std::unique_ptr<SceneNode> removeChild(SceneNode &child);
    std::vector<std::unique_ptr<SceneNode>> releaseChildren();
    void replaceChildren(std::vector<std::unique_ptr<SceneNode>> children);

    template <std::derived_from<SceneNode> T, typename... Args>
    T &emplaceChild(Args &&...args) {
        auto child = std::make_unique<T>(std::forward<Args>(args)...);
        T &ref = *child;
        appendChild(std::move(child));
        return ref;
    }

    virtual Rect localBounds() const noexcept;
    virtual void render(Renderer &renderer) const;

  private:
    void adoptChild(std::unique_ptr<SceneNode> child, std::size_t index);

    SceneNodeKind kind_;
    SceneNode *parent_ = nullptr;
    std::vector<std::unique_ptr<SceneNode>> children_;
};

class GroupNode final : public SceneNode {
  public:
    explicit GroupNode(Rect bounds = {}) : SceneNode(SceneNodeKind::Group, bounds) {}
};

class RectNode final : public SceneNode {
  public:
    explicit RectNode(Rect bounds = {}, FillStyle fill = FillStyle::none(), StrokeStyle stroke = StrokeStyle::none(), CornerRadius cornerRadius = {}, ShadowStyle shadow = ShadowStyle::none()) : SceneNode(SceneNodeKind::Rect, bounds), fill(std::move(fill)), stroke(std::move(stroke)), cornerRadius(cornerRadius), shadow(shadow) {}

    FillStyle fill = FillStyle::none();
    StrokeStyle stroke = StrokeStyle::none();
    CornerRadius cornerRadius {};
    ShadowStyle shadow = ShadowStyle::none();

    void render(Renderer &renderer) const override;
};

class TextNode final : public SceneNode {
  public:
    explicit TextNode(Rect bounds = {}, std::shared_ptr<TextLayout const> layout = {}, Point origin = {}) : SceneNode(SceneNodeKind::Text, bounds), layout(std::move(layout)), origin(origin) {}

    std::shared_ptr<TextLayout const> layout;
    Point origin {};

    void render(Renderer &renderer) const override;
};

class LineNode final : public SceneNode {
  public:
    explicit LineNode(Rect bounds = {}, Point from = {}, Point to = {}, StrokeStyle stroke = StrokeStyle::none()) : SceneNode(SceneNodeKind::Line, bounds), from(from), to(to), stroke(std::move(stroke)) {}

    Point from {};
    Point to {};
    StrokeStyle stroke = StrokeStyle::none();

    void render(Renderer &renderer) const override;
};

class PathNode final : public SceneNode {
  public:
    explicit PathNode(Rect bounds = {}, Path path = {}, FillStyle fill = FillStyle::none(), StrokeStyle stroke = StrokeStyle::none(), ShadowStyle shadow = ShadowStyle::none()) : SceneNode(SceneNodeKind::Path, bounds), path(std::move(path)), fill(std::move(fill)), stroke(std::move(stroke)), shadow(shadow) {}

    Path path;
    FillStyle fill = FillStyle::none();
    StrokeStyle stroke = StrokeStyle::none();
    ShadowStyle shadow = ShadowStyle::none();

    void render(Renderer &renderer) const override;
};

class ImageNode final : public SceneNode {
  public:
    explicit ImageNode(Rect bounds = {}, std::shared_ptr<Image const> image = {}, ImageFillMode fillMode = ImageFillMode::Cover, CornerRadius cornerRadius = {}, float opacity = 1.f) : SceneNode(SceneNodeKind::Image, bounds), image(std::move(image)), fillMode(fillMode), cornerRadius(cornerRadius), opacity(opacity) {}

    std::shared_ptr<Image const> image;
    ImageFillMode fillMode = ImageFillMode::Cover;
    CornerRadius cornerRadius {};
    float opacity = 1.f;

    void render(Renderer &renderer) const override;
};

} // namespace scenegraph
} // namespace flux
