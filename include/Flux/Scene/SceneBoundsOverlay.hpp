#pragma once

/// \file Flux/Scene/SceneBoundsOverlay.hpp
///
/// Debug overlay: strokes bounds for each retained scene node, including non-painting groups and modifiers.

#include <Flux/Core/Types.hpp>
#include <Flux/Scene/SceneTree.hpp>

namespace flux {

class Canvas;

/// Draws semi-transparent wireframes for every node in \p tree using transformed \ref SceneNode::bounds.
void renderSceneBoundsOverlay(SceneTree const& tree, Canvas& canvas);

} // namespace flux
