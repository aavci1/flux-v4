#pragma once

#include <Flux/UI/LayoutTree.hpp>

namespace flux {

class RenderContext;

/// Emit SceneGraph nodes + EventMap from a completed \ref LayoutTree (second phase after \ref Element::layout).
void renderLayoutTree(LayoutTree const& tree, RenderContext& ctx);

/// Emit starting at \p subtreeRoot (used for subtree patching after partial rebuild).
void renderLayoutSubtree(LayoutTree const& tree, LayoutNodeId subtreeRoot, RenderContext& ctx);

} // namespace flux
