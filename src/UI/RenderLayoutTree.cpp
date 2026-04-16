#include <Flux/UI/RenderLayoutTree.hpp>

#include <Flux/Core/Cursor.hpp>
#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/RenderContext.hpp>

#include "UI/Detail/EventHelpers.hpp"

#include <cmath>

namespace flux {

using detail::eventHandlersFromModifiers;
using detail::shouldInsertHandlers;

namespace {

bool renderNode(LayoutNodeId id, LayoutTree& tree, RenderContext& ctx);

void appendSceneRoots(LayoutNodeId id, LayoutTree const& tree, std::vector<NodeId>& out) {
  LayoutNode const* node = tree.get(id);
  if (!node) {
    return;
  }
  if (!node->sceneNodes.empty()) {
    out.insert(out.end(), node->sceneNodes.begin(), node->sceneNodes.end());
    return;
  }
  for (LayoutNodeId childId : node->children) {
    appendSceneRoots(childId, tree, out);
  }
}

bool canIncrementallyRenderNode(LayoutNode const& node) {
  switch (node.kind) {
  case LayoutNode::Kind::Container:
    return node.containerTag == LayoutNode::ContainerTag::None &&
           node.containerSpec.kind == ContainerLayerSpec::Kind::Standard;
  case LayoutNode::Kind::Leaf:
    return node.element && node.element->supportsIncrementalSceneReuse();
  case LayoutNode::Kind::Composite:
    return true;
  case LayoutNode::Kind::Modifier:
    return true;
  }
  return false;
}

bool renderModifier(LayoutNode& node, LayoutTree& tree, RenderContext& ctx) {
  if (ctx.incrementalSceneReuseEnabled() && node.reusedSubtreeThisBuild) {
    return true;
  }

  if (ctx.incrementalSceneReuseEnabled()) {
    for (NodeId sceneNodeId : node.sceneNodes) {
      ctx.graph().remove(sceneNodeId);
      ctx.eventMap().remove(sceneNodeId);
    }
    node.sceneNodes.clear();
  }

  ElementModifiers const& m = *node.modifiers;
  ComponentKey const stableKey = node.componentKey;
  bool const skipPrimitiveChrome =
      node.element && node.element->leafDrawsFillStrokeShadowFromModifiers();
  bool const needBgPaint =
      !skipPrimitiveChrome && (!m.fill.isNone() || !m.stroke.isNone());
  bool const needShadowPaint = !skipPrimitiveChrome && !m.shadow.isNone();
  bool const needChrome = needBgPaint || needShadowPaint;
  bool const needTransparentHit = !needChrome && m.hasInteraction();

  if (node.modifierHasEffectLayer) {
    LayerNode layer{};
    layer.opacity = m.opacity;
    layer.transform = node.modifierLayerTransform;
    float const outerW = node.frame.width;
    float const outerH = node.frame.height;
    if (m.clip && outerW > 0.f && outerH > 0.f) {
      layer.clip = Rect{0.f, 0.f, outerW, outerH};
    }
    ctx.beginCapture(&node.sceneNodes);
    NodeId const lid = ctx.addLayer(ctx.parentLayer(), std::move(layer));
    ctx.endCapture();
    ctx.pushLayer(lid);
  }

  Rect const bgBounds = node.frame;
  bool const needHitRect = needChrome || needTransparentHit;
  bool suppressPushed = false;
  if (needHitRect) {
    FillStyle fill = FillStyle::none();
    StrokeStyle stroke = StrokeStyle::none();
    if (needBgPaint) {
      fill = m.fill;
      stroke = m.stroke;
    }
    ShadowStyle const shadowForChrome = needShadowPaint ? m.shadow : ShadowStyle::none();
    if (ctx.incrementalSceneReuseEnabled()) {
      ctx.beginCapture(&node.sceneNodes);
    }
    NodeId const rid = ctx.addRect(ctx.parentLayer(), RectNode{
        .bounds = bgBounds,
        .cornerRadius = m.cornerRadius,
        .fill = std::move(fill),
        .stroke = std::move(stroke),
        .shadow = shadowForChrome,
    });
    if (ctx.incrementalSceneReuseEnabled()) {
      ctx.endCapture();
    }
    EventHandlers h = eventHandlersFromModifiers(m, stableKey, bgBounds);
    if (needTransparentHit) {
      bool const insertedHandlers = shouldInsertHandlers(h);
      if (insertedHandlers) {
        ctx.eventMap().insert(rid, std::move(h));
      }
      ctx.pushSuppressLeafModifierEvents(insertedHandlers);
      suppressPushed = true;
    } else if (needChrome && shouldInsertHandlers(h)) {
      // Without this, painted modifier rects are hit-tested but rejected by EventMap, so hover misses
      // the gap outside leaf geometry (e.g. padded row area).
      ctx.eventMap().insert(rid, std::move(h));
      ctx.pushSuppressLeafModifierEvents(true);
      suppressPushed = true;
    }
  }

  ctx.pushActiveElementModifiers(&m);
  for (LayoutNodeId c : node.children) {
    (void)renderNode(c, tree, ctx);
  }
  ctx.popActiveElementModifiers();

  if (suppressPushed) {
    ctx.popSuppressLeafModifierEvents();
  }

  if (node.modifierHasEffectLayer) {
    ctx.popLayer();
  }
  return false;
}

bool renderContainer(LayoutNode& node, LayoutTree& tree, RenderContext& ctx) {
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
  NodeId lid = kInvalidNodeId;
  bool layerStable = false;
  if (ctx.incrementalSceneReuseEnabled() && node.sceneNodes.size() == 1) {
    lid = node.sceneNodes.front();
    if (LayerNode* existing = ctx.graph().node<LayerNode>(lid)) {
      existing->transform = layer.transform;
      existing->clip = layer.clip;
      layerStable = true;
    } else {
      lid = kInvalidNodeId;
      node.sceneNodes.clear();
    }
  }
  if (!lid.isValid()) {
    node.sceneNodes.clear();
    ctx.beginCapture(&node.sceneNodes);
    lid = ctx.addLayer(ctx.parentLayer(), std::move(layer));
    ctx.endCapture();
  }
  ctx.pushLayer(lid);

  if (node.containerTag == LayoutNode::ContainerTag::PopoverCalloutShape) {
    Rect const full{0.f, 0.f, node.frame.width, node.frame.height};
    NodeId const blockId = ctx.addRect(ctx.parentLayer(), RectNode{
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

  bool childRootsStable = true;
  for (LayoutNodeId c : node.children) {
    childRootsStable &= renderNode(c, tree, ctx);
  }
  if (ctx.incrementalSceneReuseEnabled() && (!layerStable || !childRootsStable)) {
    std::vector<NodeId> ordered;
    for (LayoutNodeId c : node.children) {
      appendSceneRoots(c, tree, ordered);
    }
    ctx.graph().reorder(lid, ordered);
  }
  ctx.popLayer();
  return layerStable;
}

bool renderNode(LayoutNodeId id, LayoutTree& tree, RenderContext& ctx) {
  LayoutNode* np = tree.get(id);
  if (!np) {
    return false;
  }
  LayoutNode& node = *np;

  if (ctx.incrementalSceneReuseEnabled() && node.reusedSubtreeThisBuild) {
    return true;
  }

  switch (node.kind) {
  case LayoutNode::Kind::Container:
    return renderContainer(node, tree, ctx);
  case LayoutNode::Kind::Modifier:
    return renderModifier(node, tree, ctx);
  case LayoutNode::Kind::Leaf:
  case LayoutNode::Kind::Composite:
    if (node.element) {
      ctx.pushConstraints(node.constraints, node.hints);
      bool const hadSceneNodes = !node.sceneNodes.empty();
      bool const reused = ctx.incrementalSceneReuseEnabled() && !node.sceneNodes.empty() &&
                          node.element->reuseSceneFromLayout(ctx, node);
      if (!reused) {
        if (ctx.incrementalSceneReuseEnabled()) {
          for (NodeId sceneNodeId : node.sceneNodes) {
            ctx.graph().remove(sceneNodeId);
            ctx.eventMap().remove(sceneNodeId);
          }
        }
        node.sceneNodes.clear();
        ctx.beginCapture(&node.sceneNodes);
        node.element->renderFromLayout(ctx, node);
        ctx.endCapture();
      }
      ctx.popConstraints();
      return reused && hadSceneNodes;
    }
    return !node.sceneNodes.empty();
  }
  return false;
}

} // namespace

bool canIncrementallyRenderLayoutTree(LayoutTree const& tree) {
  for (LayoutNode const& node : tree.nodes()) {
    if (!canIncrementallyRenderNode(node)) {
      return false;
    }
  }
  return true;
}

void renderLayoutTree(LayoutTree& tree, RenderContext& ctx) {
  LayoutNodeId const root = tree.root();
  if (!root.isValid()) {
    return;
  }
  (void)renderNode(root, tree, ctx);
}

} // namespace flux
