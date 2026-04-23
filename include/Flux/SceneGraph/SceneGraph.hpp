#pragma once

/// \file Flux/SceneGraph/SceneGraph.hpp
///
/// Pure scene-graph container. Owns a single root node.

#include <memory>

namespace flux::scenegraph {

class SceneNode;

class SceneGraph {
  public:
    SceneGraph();
    ~SceneGraph();
    explicit SceneGraph(std::unique_ptr<SceneNode> root);

    SceneGraph(SceneGraph const &) = delete;
    SceneGraph &operator=(SceneGraph const &) = delete;
    SceneGraph(SceneGraph &&) = delete;
    SceneGraph &operator=(SceneGraph &&) = delete;

    SceneNode &root() noexcept;
    SceneNode const &root() const noexcept;

    void setRoot(std::unique_ptr<SceneNode> root);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace flux::scenegraph
