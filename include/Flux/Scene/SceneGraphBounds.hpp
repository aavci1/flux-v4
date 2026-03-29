#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Scene/NodeId.hpp>

namespace flux {

class SceneGraph;

/// Axis-aligned union of drawable bounds for nodes under \p subtreeRoot, in the same coordinate space
/// as \p parentWorld (typically identity for window-space overlay / main scene roots).
Rect unionSubtreeBounds(SceneGraph const& graph, NodeId subtreeRoot, Mat3 parentWorld);

/// Outer width/height of `unionSubtreeBounds` for root's direct children (typical overlay content size).
Size measureRootContentSize(SceneGraph const& graph);

} // namespace flux
