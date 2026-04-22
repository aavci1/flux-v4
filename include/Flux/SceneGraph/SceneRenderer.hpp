#pragma once

/// \file Flux/SceneGraph/SceneRenderer.hpp
///
/// Recursive renderer for the pure scene graph.

#include <memory>

namespace flux {

class Canvas;

namespace scenegraph {

class Renderer;
class SceneGraph;
class SceneNode;

class SceneRenderer {
public:
  explicit SceneRenderer(Canvas& canvas);
  explicit SceneRenderer(Renderer& renderer);
  ~SceneRenderer();

  SceneRenderer(SceneRenderer const&) = delete;
  SceneRenderer& operator=(SceneRenderer const&) = delete;
  SceneRenderer(SceneRenderer&&) = delete;
  SceneRenderer& operator=(SceneRenderer&&) = delete;

  void render(SceneGraph const& graph);
  void render(SceneNode const& node);

private:
  void renderNode(SceneNode const& node);

  Renderer* renderer_ = nullptr;
  std::unique_ptr<Renderer> ownedRenderer_;
};

} // namespace scenegraph
} // namespace flux
