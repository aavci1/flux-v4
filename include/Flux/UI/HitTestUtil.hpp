#pragma once

/// \file Flux/UI/HitTestUtil.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/EventMap.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/Scene/HitTester.hpp>
#include <Flux/Core/Types.hpp>

#include <optional>

namespace flux {

/// Prefer non-cursorPassthrough targets; fall back to all targets if none found.
std::optional<HitResult> hitTestPointerTarget(EventMap const& em, SceneGraph const& graph,
                                              Point windowPoint);

} // namespace flux
