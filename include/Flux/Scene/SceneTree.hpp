#pragma once

/// \file Flux/Scene/SceneTree.hpp
///
/// Part of the Flux public API.

#include <Flux/Scene/LocalId.hpp>
#include <Flux/Scene/Renderer.hpp>
#include <Flux/Scene/SceneNode.hpp>

#include <memory>

namespace flux {

class Canvas;

class SceneTree {
public:
  SceneTree();
  explicit SceneTree(std::unique_ptr<SceneNode> root);
  ~SceneTree();

  SceneTree(SceneTree&&) noexcept;
  SceneTree& operator=(SceneTree&&) noexcept;

  SceneTree(SceneTree const&) = delete;
  SceneTree& operator=(SceneTree const&) = delete;

  SceneNode& root() noexcept { return *root_; }
  SceneNode const& root() const noexcept { return *root_; }

  [[nodiscard]] std::unique_ptr<SceneNode> takeRoot();
  void setRoot(std::unique_ptr<SceneNode> root);
  void clear();

  static NodeId childId(NodeId parent, LocalId local) noexcept;

private:
  std::unique_ptr<SceneNode> root_;
};

void render(SceneNode& node, Renderer& renderer);
void render(SceneNode const& node, Renderer& renderer);
void render(SceneTree& tree, Renderer& renderer);
void render(SceneTree const& tree, Renderer& renderer);
void render(SceneTree& tree, Canvas& canvas);
void render(SceneTree const& tree, Canvas& canvas);

Rect measureRootContentBounds(SceneTree const& tree);
Size measureRootContentSize(SceneTree const& tree);

} // namespace flux
