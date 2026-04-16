#include <Flux/UI/Detail/RenderComponentEmit.hpp>

#include <Flux/Core/Cursor.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/UI/RenderContext.hpp>

#include <utility>

namespace flux::detail {

NodeId emitCustomRenderNode(RenderContext& ctx, Rect frame, std::function<void(Canvas&)> draw) {
  return ctx.addCustomRender(ctx.parentLayer(), CustomRenderNode{
      .frame = frame,
      .draw = std::move(draw),
  });
}

void registerRenderLeafEvents(RenderContext& ctx, NodeId id, EventHandlers handlers) {
  if (handlers.onTap || handlers.onPointerDown || handlers.onPointerUp || handlers.onPointerMove ||
      handlers.onScroll || handlers.onKeyDown || handlers.onKeyUp || handlers.onTextInput ||
      handlers.focusable || handlers.cursor != Cursor::Inherit) {
    ctx.eventMap().insert(id, std::move(handlers));
  }
}

} // namespace flux::detail
