#pragma once

/// \file Flux/UI/Detail/LayoutDebugDump.hpp
///
/// Opt-in layout tree dump when \c FLUX_DEBUG_LAYOUT is set (stderr). Internal API.

#include <Flux/Debug/DebugFlags.hpp>
#include <Flux/Core/Geometry.hpp>
#include <Flux/Layout/LayoutEngine.hpp>

namespace flux {

namespace scenegraph {
class SceneGraph;
}

namespace detail {
void layoutDebugRecordMeasureSlow(LayoutConstraints const& constraints, Size sz);
} // namespace detail

bool layoutDebugEnabled();
void layoutDebugBeginPass();
void layoutDebugEndPass();
void layoutDebugDumpRetained(scenegraph::SceneGraph const& graph);
void layoutDebugAttachSceneGraph(scenegraph::SceneGraph const* graph);
void layoutDebugDumpAttached(char const* reason);

inline void layoutDebugRecordMeasure(LayoutConstraints const& constraints, Size sz) {
  if (!debug::layoutEnabled()) {
    return;
  }
  detail::layoutDebugRecordMeasureSlow(constraints, sz);
}

} // namespace flux
