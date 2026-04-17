#pragma once

/// \file Flux/Scene/SceneTreeDump.hpp
///
/// Part of the Flux public API.

#include <iosfwd>

namespace flux {

class SceneNode;
class SceneTree;

void dumpSceneTree(SceneTree const& tree, std::ostream& os);
void dumpSceneNode(SceneNode const& node, std::ostream& os, int depth = 0);

} // namespace flux
