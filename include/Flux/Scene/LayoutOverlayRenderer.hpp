#pragma once

/// \file Flux/Scene/LayoutOverlayRenderer.hpp
///
/// Debug overlay: strokes bounds for each layout node, including non-drawing containers such as
/// stacks and other structural nodes.

#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutTree.hpp>

namespace flux {

class Canvas;

/// Draws semi-transparent wireframes for every node in \p tree using \ref LayoutNode::worldBounds.
void renderLayoutOverlay(LayoutTree const& tree, Canvas& canvas);

} // namespace flux
