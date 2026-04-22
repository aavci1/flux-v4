#include <Flux/UI/SceneBuilder.hpp>

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/SceneTree.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/StateStore.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

#include "UI/Build/ComponentBuildContext.hpp"
#include "UI/Build/ComponentBuildSupport.hpp"
#include "UI/SceneBuilder/ElementEnvelope.hpp"
#include "UI/SceneBuilder/MeasureLayoutCache.hpp"
#include "UI/SceneBuilder/NodeReuse.hpp"

namespace flux {

namespace build_support = ::flux::detail::build;
namespace scene_builder = ::flux::detail::scene_builder;

SceneBuilder::SceneBuilder(TextSystem& textSystem, EnvironmentStack& environment,
                           SceneGeometryIndex* geometryIndex)
    : textSystem_(textSystem)
    , environment_(environment)
    , geometryIndex_(geometryIndex)
    , measureLayoutCache_(std::make_unique<detail::MeasureLayoutCache>()) {}

SceneBuilder::~SceneBuilder() = default;

detail::TraversalContext::Frame const& SceneBuilder::frame() const {
  return traversal_.frame();
}

void SceneBuilder::pushFrame(LayoutConstraints const& constraints, LayoutHints const& hints, Point origin,
                             ComponentKey key, Size assignedSize, bool hasAssignedWidth,
                             bool hasAssignedHeight) {
  traversal_.pushFrame(constraints, hints, origin, std::move(key), assignedSize, hasAssignedWidth,
                       hasAssignedHeight);
  ++buildFrameDepth_;
}

void SceneBuilder::popFrame() {
  traversal_.popFrame();
  if (buildFrameDepth_ > 0) {
    --buildFrameDepth_;
  }
}

Size SceneBuilder::measureElement(Element const& el, LayoutConstraints const& constraints,
                                  LayoutHints const& hints, ComponentKey const& key) const {
  ++lastBuildStats_.measuredNodes;
  detail::MeasureLayoutKey const cacheKey{
      .measureId = el.measureId(),
      .componentKey = key,
      .constraints = constraints,
      .hints = hints,
  };
  if (measureLayoutCache_) {
    if (Size const* cached = measureLayoutCache_->findElementSize(cacheKey)) {
      return *cached;
    }
  }
  MeasureContext measureContext{textSystem_, measureLayoutCache_.get()};
  measureContext.pushConstraints(constraints, hints);
  measureContext.resetTraversalState(key);
  if (el.expandsBody()) {
    measureContext.setMeasurementRootKey(key);
  } else {
    measureContext.clearMeasurementRootKey();
  }
  measureContext.setCurrentElement(&el);
  Size const measured = el.measure(measureContext, constraints, hints, textSystem_);
  if (measureLayoutCache_) {
    measureLayoutCache_->recordElementSize(cacheKey, measured);
  }
  return measured;
}

std::unique_ptr<SceneNode> SceneBuilder::build(Element const& el, NodeId id,
                                               LayoutConstraints const& constraints,
                                               std::unique_ptr<SceneNode> existing,
                                               ComponentKey rootKey) {
  return build(el, id, constraints, std::move(existing), std::move(rootKey), true, true);
}

std::unique_ptr<SceneNode> SceneBuilder::build(Element const& el, NodeId id,
                                               LayoutConstraints const& constraints,
                                               std::unique_ptr<SceneNode> existing,
                                               ComponentKey rootKey,
                                               bool rootUsesMaxWidthAsAssigned,
                                               bool rootUsesMaxHeightAsAssigned) {
  measureLayoutCache_->clear();
  lastBuildStats_ = {};
  if (geometryIndex_) {
    geometryIndex_->beginBuild();
  }
  Size rootAssigned{};
  bool const hasRootWidth = rootUsesMaxWidthAsAssigned && std::isfinite(constraints.maxWidth);
  bool const hasRootHeight = rootUsesMaxHeightAsAssigned && std::isfinite(constraints.maxHeight);
  if (hasRootWidth) {
    rootAssigned.width = std::max(0.f, constraints.maxWidth);
  }
  if (hasRootHeight) {
    rootAssigned.height = std::max(0.f, constraints.maxHeight);
  }
  pushFrame(constraints, LayoutHints{}, Point{}, std::move(rootKey), rootAssigned, hasRootWidth, hasRootHeight);
  std::unique_ptr<SceneNode> node = buildOrReuse(el, id, std::move(existing));
  popFrame();
  if (geometryIndex_) {
    geometryIndex_->finishBuild();
  }
  return node;
}

std::unique_ptr<SceneNode> SceneBuilder::buildOrReuse(Element const& el, NodeId id,
                                                      std::unique_ptr<SceneNode> existing) {
  if (buildFrameDepth_ == 0) {
    return build(el, id, LayoutConstraints{}, std::move(existing));
  }

  ++lastBuildStats_.resolvedNodes;
  detail::ResolvedElement resolved = el.resolve(frame().key, frame().constraints);

  auto retainExisting = [&](std::unique_ptr<SceneNode> node) -> std::unique_ptr<SceneNode> {
    StateStore* const store = StateStore::current();
    if (!node) {
      return nullptr;
    }
    node->position = node->retainedBuildStamp().localPosition;
    if (store) {
      store->markRetainedSubtreeVisited(frame().key);
    }
    if (geometryIndex_) {
      Point delta{};
      if (!frame().key.empty()) {
        if (std::optional<Rect> previousRect = geometryIndex_->forKey(frame().key)) {
          delta = Point{frame().origin.x - previousRect->x, frame().origin.y - previousRect->y};
        }
      }
      geometryIndex_->retainSubtree(frame().key, delta);
    }
    return node;
  };

  if (existing && canRetainExistingSubtree(resolved, *existing)) {
    ++lastBuildStats_.skippedNodes;
    return retainExisting(std::move(existing));
  }

  return buildResolved(el, resolved, id, std::move(existing));
}

void SceneBuilder::reconcileChildren(SceneNode& parent, std::span<Element const> newChildren,
                                     std::vector<std::unique_ptr<SceneNode>>& existingChildren) {
  detail::ReusableSceneNodes reusable = detail::collectReusableSceneNodes(std::move(existingChildren));

  std::vector<std::unique_ptr<SceneNode>> next{};
  next.reserve(newChildren.size());
  for (std::size_t i = 0; i < newChildren.size(); ++i) {
    Element const& child = newChildren[i];
    NodeId const childId = SceneTree::childId(parent.id(), build_support::childLocalId(child, i));
    std::unique_ptr<SceneNode> reuse = detail::takeReusableNode(reusable, childId);
    next.push_back(buildOrReuse(child, childId, std::move(reuse)));
  }
  parent.replaceChildren(std::move(next));
}

std::unique_ptr<SceneNode> SceneBuilder::buildResolved(Element const& el, detail::ResolvedElement const& resolved,
                                                       NodeId id, std::unique_ptr<SceneNode> existing) {
  auto const current = frame();
  ++lastBuildStats_.materializedNodes;
  if (!resolved.sceneElement) {
    auto root = std::make_unique<SceneNode>(id);
    root->bounds = build_support::sizeRect(measureElement(el, current.constraints, current.hints, current.key));
    stampRetainedBuild(*root, resolved);
    ++lastBuildStats_.arrangedNodes;
    return root;
  }

  // `measureElement(el, ...)` may resolve and replace the same cached body again, so keep a
  // local copy of the resolved scene element while this build call runs.
  Element stableSceneEl = *resolved.sceneElement;
  ElementType const typeTag = stableSceneEl.typeTag();

  std::vector<detail::ElementModifiers> const& modifierLayers = resolved.modifierLayers;
  detail::ElementModifiers const* mods = modifierLayers.empty() ? nullptr : &modifierLayers.back();
  bool const leafOwnsModifierPaint = stableSceneEl.leafDrawsFillStrokeShadowFromModifiers();
  Point const subtreeOffset = scene_builder::modifierOffset(mods);
  EdgeInsets const padding = mods ? mods->padding : EdgeInsets{};
  LayoutConstraints innerConstraints = scene_builder::insetConstraints(current.constraints, padding);
  Size contentAssignedSize = current.assignedSize;
  if (current.hasAssignedWidth) {
    contentAssignedSize.width = std::max(0.f, current.assignedSize.width - std::max(0.f, padding.left) -
                                                  std::max(0.f, padding.right));
  }
  if (current.hasAssignedHeight) {
    contentAssignedSize.height = std::max(0.f, current.assignedSize.height - std::max(0.f, padding.top) -
                                                   std::max(0.f, padding.bottom));
  }
  Point const contentOrigin{
      current.origin.x + subtreeOffset.x + std::max(0.f, padding.left),
      current.origin.y + subtreeOffset.y + std::max(0.f, padding.top),
  };

  auto recordGeometry = [&](Size size) {
    if (geometryIndex_) {
      Rect const rect{current.origin.x + subtreeOffset.x, current.origin.y + subtreeOffset.y,
                      std::max(0.f, size.width), std::max(0.f, size.height)};
      geometryIndex_->record(current.key, rect);
      for (ComponentKey const& bodyKey : resolved.bodyComponentKeys) {
        geometryIndex_->record(bodyKey, rect);
      }
    }
  };

  std::unique_ptr<SceneNode> innerExisting = std::move(existing);
  std::vector<scene_builder::DecorationReuse> decorationReuse(modifierLayers.size());
  for (std::size_t i = 0; i < modifierLayers.size(); ++i) {
    detail::ElementModifiers const& layer = modifierLayers[i];
    bool const isInnermost = i + 1 == modifierLayers.size();
    decorationReuse[i] = scene_builder::takeDecorationReuse(
        innerExisting, &layer, isInnermost ? leafOwnsModifierPaint : false,
        isInnermost || scene_builder::hasPadding(&layer) || scene_builder::hasOverlay(&layer));
  }

  std::unique_ptr<ModifierSceneNode> modifierWrapper{};
  std::unique_ptr<SceneNode> layoutWrapper{};
  std::unique_ptr<SceneNode> overlayNode{};
  if (!decorationReuse.empty()) {
    modifierWrapper = std::move(decorationReuse.back().modifierWrapper);
    layoutWrapper = std::move(decorationReuse.back().layoutWrapper);
    overlayNode = std::move(decorationReuse.back().overlay);
  }

  scene_builder::EnvironmentLayerScope resolvedEnvironment{environment_, resolved.environmentLayers};

  detail::ComponentBuildContext buildContext{
      *this,
      traversal_,
      el,
      stableSceneEl,
      typeTag,
      id,
      mods,
      modifierLayers.size() > 1,
      innerConstraints,
      contentOrigin,
      contentAssignedSize,
  };

  detail::ComponentBuildResult built = stableSceneEl.buildMeasured(buildContext, std::move(innerExisting));
  std::unique_ptr<SceneNode> core = std::move(built.node);
  std::unique_ptr<InteractionData> resolvedInteraction = std::move(built.interaction);
  Size geometrySize = built.geometrySize;
  bool hasGeometrySize = built.hasGeometrySize;
  if (!core) {
    core = std::make_unique<SceneNode>(id);
  }

  Size outerSize{};
  Size layoutOuterSize{};
  buildContext.finalizeOuterSizes(hasGeometrySize ? geometrySize : build_support::rectSize(core->bounds), outerSize,
                                  layoutOuterSize);

  std::unique_ptr<InteractionData> interaction = std::move(resolvedInteraction);
  if (!interaction) {
    interaction = build_support::makeInteractionData(mods, current.key);
  }
  std::unique_ptr<SceneNode> root =
      decorateNode(std::move(core), leafOwnsModifierPaint, mods, std::move(modifierWrapper),
                   std::move(layoutWrapper),
                   std::move(overlayNode), layoutOuterSize, outerSize, subtreeOffset, padding,
                   std::move(interaction));

  for (std::size_t i = modifierLayers.size(); i > 1; --i) {
    detail::ElementModifiers const& outerLayer = modifierLayers[i - 2];
    std::unique_ptr<InteractionData> outerInteraction = build_support::makeInteractionData(&outerLayer, current.key);
    if (!scene_builder::needsDecorationPass(&outerLayer, outerSize, build_support::rectSize(root->bounds), false,
                                            outerInteraction.get())) {
      continue;
    }
    scene_builder::DecorationReuse& reuse = decorationReuse[i - 2];
    root = decorateNode(std::move(root), false, &outerLayer, std::move(reuse.modifierWrapper),
                        std::move(reuse.layoutWrapper), std::move(reuse.overlay), outerSize, outerSize,
                        scene_builder::modifierOffset(&outerLayer), outerLayer.padding,
                        std::move(outerInteraction));
  }
  if (root->bounds.width <= 0.f && root->bounds.height <= 0.f) {
    root->bounds = build_support::sizeRect(outerSize);
  }
  stampRetainedBuild(*root, resolved);
  ++lastBuildStats_.arrangedNodes;
  Size geometryRectSize = outerSize;
  if (geometryRectSize.width <= 0.f && geometryRectSize.height <= 0.f) {
    geometryRectSize = build_support::rectSize(root->bounds);
  }
  recordGeometry(geometryRectSize);
  return root;
}

} // namespace flux
