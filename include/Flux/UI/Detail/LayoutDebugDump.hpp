#pragma once

/// \file Flux/UI/Detail/LayoutDebugDump.hpp
///
/// Opt-in layout tree dump when \c FLUX_DEBUG_LAYOUT is set (stderr). Internal API.

#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <cstdint>

namespace flux {

class SceneGeometryIndex;
class SceneTree;

void layoutDebugBeginPass();
void layoutDebugEndPass();

void layoutDebugRecordMeasure(std::uint64_t measureId, LayoutConstraints const& constraints, Size sz);
void layoutDebugDumpRetained(SceneTree const& tree, SceneGeometryIndex const& geometry);

} // namespace flux
