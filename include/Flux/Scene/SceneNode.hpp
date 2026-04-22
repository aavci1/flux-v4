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
#include <utility>
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

/// Layout stamp used to short-circuit rebuilds for unchanged retained subtrees.
struct RetainedBuildStamp {
  std::uint64_t measureId = 0;
  float maxWidth = 0.f;
  float maxHeight = 0.f;
  float minWidth = 0.f;
  float minHeight = 0.f;
  float assignedWidth = 0.f;
  float assignedHeight = 0.f;
  bool hasAssignedWidth = false;
  bool hasAssignedHeight = false;
  std::int8_t hStackCrossAlign = -1;
  std::int8_t vStackCrossAlign = -1;
  std::int8_t zStackHorizontalAlign = -1;
  std::int8_t zStackVerticalAlign = -1;
  Point localPosition{};
  std::unique_ptr<void, void (*)(void*)> comparableLeafElement{nullptr, nullptr};
  std::unique_ptr<void, void (*)(void*)> comparableLeafTheme{nullptr, nullptr};
};

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
  RetainedBuildStamp const& retainedBuildStamp() const noexcept { return retainedBuildStamp_; }
  void setRetainedBuildStamp(RetainedBuildStamp stamp) noexcept { retainedBuildStamp_ = std::move(stamp); }

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
  RetainedBuildStamp retainedBuildStamp_{};
};

} // namespace flux
