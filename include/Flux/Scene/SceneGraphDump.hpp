#pragma once

/// \file Flux/Scene/SceneGraphDump.hpp
///
/// Part of the Flux public API.


#include <Flux/Scene/SceneGraph.hpp>

#include <iosfwd>

namespace flux {

/// Prints a tree of nodes under `graph.root()` to `os` (stderr by default). Useful for debugging
/// structure, ids, transforms, and geometry.
void dumpSceneGraph(SceneGraph const& graph, std::ostream& os);

} // namespace flux
