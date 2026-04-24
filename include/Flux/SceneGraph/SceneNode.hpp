#pragma once

/// \file Flux/SceneGraph/SceneNode.hpp
///
/// Pure scene-graph base node type. Each node stores parent-space bounds and renders itself in
/// local coordinates.

#include <Flux/Core/Types.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace flux {

namespace scenegraph {

class Renderer;
class PreparedRenderOps;
struct InteractionData;

namespace detail {
struct SceneNodeAccess;
}

enum class SceneNodeKind : std::uint8_t {
  Group,
  Rect,
  Text,
  Image,
  Path,
  Render,
};

std::string_view sceneNodeKindName(SceneNodeKind kind) noexcept;

class SceneNode {
  public:
    explicit SceneNode(SceneNodeKind kind, Rect bounds = {});
    virtual ~SceneNode();

    SceneNode(SceneNode const &) = delete;
    SceneNode &operator=(SceneNode const &) = delete;
    SceneNode(SceneNode &&) = delete;
    SceneNode &operator=(SceneNode &&) = delete;

    SceneNodeKind kind() const noexcept;
    Rect bounds() const noexcept;
    Point position() const noexcept;
    Size size() const noexcept;
    Mat3 const& transform() const noexcept;
    bool isDirty() const noexcept;

    void setBounds(Rect bounds);
    void setPosition(Point position);
    void setSize(Size size);
    void setTransform(Mat3 const& transform);

    SceneNode *parent() const noexcept;
    std::span<std::unique_ptr<SceneNode> const> children() const noexcept;
    std::span<std::unique_ptr<SceneNode>> children() noexcept;
    InteractionData *interaction() noexcept;
    InteractionData const *interaction() const noexcept;
    void setInteraction(std::unique_ptr<InteractionData> interaction);

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
    virtual bool canPrepareRenderOps() const noexcept;

  protected:
    void markDirty() noexcept;

  private:
    SceneNodeKind kind_;
    Rect bounds_{};
    Mat3 transform_ = Mat3::identity();
    SceneNode* parent_ = nullptr;
    std::vector<std::unique_ptr<SceneNode>> children_{};
    std::unique_ptr<InteractionData> interaction_{};
    mutable bool dirty_ = true;
    mutable bool subtreeDirty_ = true;
    mutable std::unique_ptr<PreparedRenderOps> preparedRenderOps_{};

    friend struct detail::SceneNodeAccess;
};

} // namespace scenegraph
} // namespace flux
