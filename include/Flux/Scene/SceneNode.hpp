#pragma once

/// \file Flux/Scene/SceneNode.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/Types.hpp>
#include <Flux/Scene/InteractionData.hpp>
#include <Flux/Scene/NodeId.hpp>
#include <Flux/Scene/PaintCommand.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace flux {

class Renderer;

enum class SceneNodeKind : std::uint8_t {
  Group,
  Modifier,
  Rect,
  Text,
  Image,
  Path,
  Line,
  Render,
  Custom,
};

std::string_view sceneNodeKindName(SceneNodeKind kind) noexcept;

class SceneNode {
public:
  explicit SceneNode(NodeId id);
  virtual ~SceneNode();

  SceneNodeKind kind() const noexcept { return kind_; }
  NodeId id() const noexcept { return id_; }

  Point position{};
  Rect bounds{};

  SceneNode* parent() const noexcept { return parent_; }
  std::span<std::unique_ptr<SceneNode> const> children() const noexcept { return children_; }

  void appendChild(std::unique_ptr<SceneNode> child);
  void insertChild(std::size_t index, std::unique_ptr<SceneNode> child);
  std::unique_ptr<SceneNode> removeChild(SceneNode* child);
  std::vector<std::unique_ptr<SceneNode>> releaseChildren();
  void reorderChildren(std::span<SceneNode* const> order);
  void replaceChildren(std::vector<std::unique_ptr<SceneNode>> children);

  bool paintDirty() const noexcept { return paintDirty_; }
  bool boundsDirty() const noexcept { return boundsDirty_; }
  void invalidatePaint() noexcept { paintDirty_ = true; }
  void markBoundsDirty() noexcept { boundsDirty_ = true; }

  virtual bool paints() const noexcept { return false; }
  virtual void applyNodeState(Renderer&) const {}
  virtual void replayLocalPaint(Renderer&) const;
  virtual void rebuildLocalPaint();

  virtual Rect computeOwnBounds() const;
  virtual Rect adjustSubtreeBounds(Rect r) const;
  virtual void recomputeBounds();

  virtual SceneNode* hitTest(Point local);
  virtual SceneNode const* hitTest(Point local) const;

  InteractionData* interaction() noexcept { return interaction_.get(); }
  InteractionData const* interaction() const noexcept { return interaction_.get(); }
  void setInteraction(std::unique_ptr<InteractionData> interaction);

protected:
  SceneNode(SceneNodeKind kind, NodeId id);

  void clearPaintDirty() noexcept { paintDirty_ = false; }
  void clearBoundsDirty() noexcept { boundsDirty_ = false; }
  std::vector<PaintCommand>& localPaintCache() noexcept { return localPaintCache_; }
  std::vector<PaintCommand> const& localPaintCache() const noexcept { return localPaintCache_; }

private:
  void adoptChild(std::unique_ptr<SceneNode> child, std::size_t index);

protected:
  SceneNodeKind kind_;
  NodeId id_{};
  SceneNode* parent_ = nullptr;
  std::vector<std::unique_ptr<SceneNode>> children_;
  std::unique_ptr<InteractionData> interaction_;
  bool paintDirty_ = true;
  bool boundsDirty_ = true;
  std::vector<PaintCommand> localPaintCache_{};
};

} // namespace flux
