#pragma once

/// \file Flux/UI/Detail/LayoutDebugDump.hpp
///
/// Opt-in layout tree dump when \c FLUX_DEBUG_LAYOUT is set (stderr). Internal API.

#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <cstdint>

namespace flux {

namespace scenegraph {
class SceneGraph;
}

void layoutDebugBeginPass();
void layoutDebugEndPass();

void layoutDebugRecordMeasure(std::uint64_t measureId, LayoutConstraints const& constraints, Size sz);
void layoutDebugDumpRetained(scenegraph::SceneGraph const& graph);

} // namespace flux
