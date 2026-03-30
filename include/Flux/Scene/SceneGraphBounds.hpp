#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Scene/NodeId.hpp>

namespace flux {

class SceneGraph;

/// Product of layer transforms along the path from the scene root to the parent of \p subtreeRoot.
/// Pass as \p parentWorld to `unionSubtreeBounds` so nested subtree bounds accumulate in root/window space.
Mat3 subtreeAncestorWorldTransform(SceneGraph const& graph, NodeId subtreeRoot);

/// Axis-aligned union of drawable bounds for nodes under \p subtreeRoot, in the same coordinate space
/// as \p parentWorld (typically identity for window-space overlay / main scene roots).
Rect unionSubtreeBounds(SceneGraph const& graph, NodeId subtreeRoot, Mat3 parentWorld);

/// Axis-aligned union of drawable bounds for the overlay root's children, in root-local space.
/// Used for positioning: the overlay translate maps local (0,0) to the frame origin; content may
/// occupy a sub-rectangle with non-zero min x/y, so centering must use this rect, not size alone.
Rect measureRootContentBounds(SceneGraph const& graph);

/// Convenience: `measureRootContentBounds` width/height (kept for call sites that only need size).
Size measureRootContentSize(SceneGraph const& graph);

} // namespace flux
