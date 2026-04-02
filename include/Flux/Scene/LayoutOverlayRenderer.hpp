#pragma once

/// \file Flux/Scene/LayoutOverlayRenderer.hpp
///
/// Debug overlay: strokes layout bounds for each scene node (matches \ref SceneRenderer transform stack).

#include <Flux/Core/Types.hpp>
#include <Flux/Scene/NodeId.hpp>

namespace flux {

class Canvas;
class SceneGraph;

/// Draws semi-transparent wireframes for \p RectNode, \p TextNode, \p ImageNode, \p CustomRenderNode,
/// \p PathNode, and \p LineNode bounds in the same coordinate space as \ref SceneRenderer.
void renderLayoutOverlay(SceneGraph const& graph, Canvas& canvas);

} // namespace flux
