#include <Flux/UI/Views/Render.hpp>

#include <Flux/UI/Detail/LeafBounds.hpp>
#include <Flux/UI/LayoutContext.hpp>

namespace flux {

void Render::layout(LayoutContext& ctx) const {
  ComponentKey const stableKey = ctx.leafComponentKey();
  ctx.advanceChildSlot();
  Rect const frame =
      detail::resolveLeafLayoutBounds({}, ctx.layoutEngine().consumeAssignedFrame(), ctx.constraints(), ctx.hints());
  LayoutNode node{};
  node.kind = LayoutNode::Kind::Leaf;
  node.frame = frame;
  node.componentKey = stableKey;
  node.element = ctx.currentElement();
  node.constraints = ctx.constraints();
  node.hints = ctx.hints();
  LayoutNodeId const id = ctx.pushLayoutNode(std::move(node));
  ctx.registerCompositeSubtreeRootIfPending(id);
}

Size Render::measure(LayoutContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
                     TextSystem&) const {
  ctx.advanceChildSlot();
  return measure(constraints, hints);
}

} // namespace flux
