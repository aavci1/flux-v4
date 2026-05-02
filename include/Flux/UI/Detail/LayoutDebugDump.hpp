#pragma once

/// \file Flux/UI/Detail/LayoutDebugDump.hpp
///
/// Opt-in layout tree dump when \c FLUX_DEBUG_LAYOUT is set (stderr). Internal API.

#include <Flux/Debug/DebugFlags.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutEngine.hpp>

namespace flux {

namespace scenegraph {
class SceneGraph;
}

namespace detail {
void layoutDebugRecordMeasureSlow(LayoutConstraints const& constraints, Size sz);
} // namespace detail

void layoutDebugBeginPass();
void layoutDebugEndPass();

inline void layoutDebugRecordMeasure(LayoutConstraints const& constraints, Size sz) {
  if (!debug::layoutEnabled()) {
    return;
  }
  detail::layoutDebugRecordMeasureSlow(constraints, sz);
}

void layoutDebugDumpRetained(scenegraph::SceneGraph const& graph);
void layoutDebugAttachSceneGraph(scenegraph::SceneGraph const* graph);
void layoutDebugDumpAttached(char const* reason);

} // namespace flux
