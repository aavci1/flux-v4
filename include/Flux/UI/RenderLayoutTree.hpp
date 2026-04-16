#pragma once

#include <Flux/UI/LayoutTree.hpp>

namespace flux {

class RenderContext;

/// Emit SceneGraph nodes + EventMap from a completed \ref LayoutTree (second phase after \ref Element::layout).
void renderLayoutTree(LayoutTree& tree, RenderContext& ctx);

} // namespace flux
