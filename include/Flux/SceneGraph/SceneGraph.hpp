#pragma once

/// \file Flux/SceneGraph/SceneGraph.hpp
///
/// Pure scene-graph container. Owns a single root node.

#include <Flux/SceneGraph/SceneNode.hpp>

#include <memory>

namespace flux::scenegraph {

class SceneGraph {
public:
  SceneGraph();
  explicit SceneGraph(std::unique_ptr<SceneNode> root);

  SceneNode& root() noexcept { return *root_; }
  SceneNode const& root() const noexcept { return *root_; }

  void setRoot(std::unique_ptr<SceneNode> root);

private:
  std::unique_ptr<SceneNode> root_;
};

} // namespace flux::scenegraph
