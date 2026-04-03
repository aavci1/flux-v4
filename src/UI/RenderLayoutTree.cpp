#include <Flux/UI/RenderLayoutTree.hpp>

#include <Flux/Core/Cursor.hpp>
#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/RenderContext.hpp>

#include <cmath>

namespace flux {

namespace {

EventHandlers eventHandlersFromModifiers(ElementModifiers const& m, ComponentKey stableKey) {
  bool const effFocusable =
      m.focusable || static_cast<bool>(m.onKeyDown) || static_cast<bool>(m.onKeyUp) ||
      static_cast<bool>(m.onTextInput);
  return EventHandlers{
      .stableTargetKey = stableKey,
      .onTap = m.onTap,
      .onPointerDown = m.onPointerDown,
      .onPointerUp = m.onPointerUp,
      .onPointerMove = m.onPointerMove,
      .onScroll = m.onScroll,
      .onKeyDown = m.onKeyDown,
      .onKeyUp = m.onKeyUp,
      .onTextInput = m.onTextInput,
      .focusable = effFocusable,
      .cursor = m.cursor,
  };
}

bool shouldInsertHandlers(EventHandlers const& h) {
  return static_cast<bool>(h.onTap) || static_cast<bool>(h.onPointerDown) || static_cast<bool>(h.onPointerUp) ||
         static_cast<bool>(h.onPointerMove) || static_cast<bool>(h.onScroll) || static_cast<bool>(h.onKeyDown) ||
         static_cast<bool>(h.onKeyUp) || static_cast<bool>(h.onTextInput) || h.focusable ||
         h.cursor != Cursor::Inherit;
}

void renderNode(LayoutNodeId id, LayoutTree const& tree, RenderContext& ctx);

void renderModifier(LayoutNode const& node, LayoutTree const& tree, RenderContext& ctx) {
  ElementModifiers const& m = *node.modifiers;
  ComponentKey const stableKey = node.componentKey;
  bool const needBg = !m.background.isNone() || !m.border.isNone();
  bool const needTransparentHit = !needBg && m.hasInteraction();

  if (node.modifierHasEffectLayer) {
    LayerNode layer{};
    layer.opacity = m.opacity;
    layer.transform = node.modifierLayerTransform;
    float const outerW = node.frame.width;
    float const outerH = node.frame.height;
    if (m.clip && outerW > 0.f && outerH > 0.f) {
      layer.clip = Rect{0.f, 0.f, outerW, outerH};
    }
    NodeId const lid = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
    ctx.pushLayer(lid);
  }

  Rect const bgBounds = node.frame;
  bool const needHitRect = needBg || needTransparentHit;
  if (needHitRect) {
    FillStyle fill = FillStyle::none();
    StrokeStyle stroke = StrokeStyle::none();
    if (needBg) {
      fill = m.background;
      stroke = m.border;
    }
    NodeId const rid = ctx.graph().addRect(ctx.parentLayer(), RectNode{
        .bounds = bgBounds,
        .cornerRadius = m.cornerRadius,
        .fill = std::move(fill),
        .stroke = std::move(stroke),
    });
    if (needTransparentHit) {
      EventHandlers const h = eventHandlersFromModifiers(m, stableKey);
      bool const insertedHandlers = shouldInsertHandlers(h);
      if (insertedHandlers) {
        ctx.eventMap().insert(rid, h);
      }
      ctx.pushSuppressLeafModifierEvents(insertedHandlers);
    }
  }

  ctx.pushActiveElementModifiers(&m);
  for (LayoutNodeId c : node.children) {
    renderNode(c, tree, ctx);
  }
  ctx.popActiveElementModifiers();

  if (needTransparentHit) {
    ctx.popSuppressLeafModifierEvents();
  }

  if (node.modifierHasEffectLayer) {
    ctx.popLayer();
  }
}

void renderContainer(LayoutNode const& node, LayoutTree const& tree, RenderContext& ctx) {
  LayerNode layer{};
  switch (node.containerSpec.kind) {
  case ContainerLayerSpec::Kind::Standard:
    layer.transform = Mat3::translate(node.frame.x, node.frame.y);
    if (node.containerSpec.clip && node.containerSpec.clipW > 0.f && node.containerSpec.clipH > 0.f) {
      layer.clip = Rect{0.f, 0.f, node.containerSpec.clipW, node.containerSpec.clipH};
    }
    break;
  case ContainerLayerSpec::Kind::OffsetScroll: {
    float const ox = node.frame.x - node.containerSpec.scrollOffset.x;
    float const oy = node.frame.y - node.containerSpec.scrollOffset.y;
    layer.transform = Mat3::translate(ox, oy);
    break;
  }
  case ContainerLayerSpec::Kind::ScaleAroundCenter:
    layer.transform = node.containerSpec.customTransform;
    break;
  }
  NodeId const lid = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
  ctx.pushLayer(lid);

  if (node.containerTag == LayoutNode::ContainerTag::PopoverCalloutShape) {
    Rect const full{0.f, 0.f, node.frame.width, node.frame.height};
    NodeId const blockId = ctx.graph().addRect(ctx.parentLayer(), RectNode{
        .bounds = full,
        .fill = FillStyle::none(),
        .stroke = StrokeStyle::none(),
    });
    ctx.eventMap().insert(blockId, EventHandlers{
        .stableTargetKey = {},
        .onScroll = [](Vec2) {},
        .cursor = Cursor::Arrow,
    });
  }

  for (LayoutNodeId c : node.children) {
    renderNode(c, tree, ctx);
  }
  ctx.popLayer();
}

void renderNode(LayoutNodeId id, LayoutTree const& tree, RenderContext& ctx) {
  LayoutNode const* np = tree.get(id);
  if (!np) {
    return;
  }
  LayoutNode const& node = *np;

  switch (node.kind) {
  case LayoutNode::Kind::Container:
    renderContainer(node, tree, ctx);
    break;
  case LayoutNode::Kind::Modifier:
    renderModifier(node, tree, ctx);
    break;
  case LayoutNode::Kind::Leaf:
  case LayoutNode::Kind::Composite:
    if (node.element) {
      ctx.pushConstraints(node.constraints, node.hints);
      node.element->renderFromLayout(ctx, node);
      ctx.popConstraints();
    }
    break;
  }
}

} // namespace

void renderLayoutTree(LayoutTree const& tree, RenderContext& ctx) {
  LayoutNodeId const root = tree.root();
  if (!root.isValid()) {
    return;
  }
  renderNode(root, tree, ctx);
}

} // namespace flux
