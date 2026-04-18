#pragma once

/// \file Flux/Scene/Render.hpp
///
/// Part of the Flux public API.

#include <memory>

namespace flux {

class Canvas;
class Renderer;
class SceneNode;
class SceneTree;

void render(SceneNode& node, Renderer& renderer);
void render(SceneNode const& node, Renderer& renderer);
void render(SceneTree& tree, Renderer& renderer);
void render(SceneTree const& tree, Renderer& renderer);
void render(SceneTree& tree, Canvas& canvas);
void render(SceneTree const& tree, Canvas& canvas);

} // namespace flux
