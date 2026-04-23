#pragma once

/// \file Flux/Scene/SceneTree.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/LocalId.hpp>
#include <Flux/Scene/SceneNode.hpp>

#include <memory>

namespace flux {

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

Rect measureRootContentBounds(SceneTree const& tree);
Size measureRootContentSize(SceneTree const& tree);

} // namespace flux
