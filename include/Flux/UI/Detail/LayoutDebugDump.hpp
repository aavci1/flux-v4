#pragma once

/// \file Flux/UI/Detail/LayoutDebugDump.hpp
///
/// Opt-in layout tree dump when \c FLUX_DEBUG_LAYOUT is set (stderr). Internal API.

#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <cstdint>

namespace flux {

void layoutDebugBeginPass();
void layoutDebugEndPass();

void layoutDebugPushElementBuild(std::uint64_t elementMeasureId);
void layoutDebugPopElementBuild();

void layoutDebugRecordMeasure(std::uint64_t measureId, LayoutConstraints const& constraints, Size sz);

void layoutDebugLogContainer(char const* tag, LayoutConstraints const& outer, Rect parentFrame);

void layoutDebugLogLeaf(char const* tag, LayoutConstraints const& cs, Rect frame, float flexGrow,
                        float flexShrink, float minMain);

} // namespace flux
