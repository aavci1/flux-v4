#pragma once

/// \file Flux/UI/Detail/RenderComponentEmit.hpp
///
/// Non-template boundary for RenderComponent scene-graph emission.
/// Intentionally does NOT include Canvas.hpp, SceneGraph.hpp, or Nodes.hpp so that
/// Element.hpp (which is included by every view header) avoids propagating those headers.

#include <Flux/Core/Types.hpp>
#include <Flux/Core/Cursor.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Scene/NodeId.hpp>
#include <Flux/UI/EventMap.hpp>

#include <functional>

namespace flux {
class RenderContext;
} // namespace flux

namespace flux::detail {

/// Emit a CustomRenderNode into \p ctx and return its NodeId.
NodeId emitCustomRenderNode(RenderContext& ctx, Rect frame, std::function<void(Canvas&)> draw);

/// Insert \p handlers into the EventMap if any handler field is set.
void registerRenderLeafEvents(RenderContext& ctx, NodeId id, EventHandlers handlers);

} // namespace flux::detail
