#include <Flux/UI/SceneBuilder.hpp>

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/StateStore.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "UI/Build/ComponentBuildContext.hpp"
#include "UI/Build/ComponentBuildSupport.hpp"
#include "UI/Layout/LayoutHelpers.hpp"
#include "UI/SceneBuilder/MeasureLayoutCache.hpp"
#include "SceneGraph/SceneBounds.hpp"

namespace flux {

namespace build = ::flux::detail::build;

namespace {

struct EnvironmentLayerScope {
  EnvironmentStack& environment;
  std::size_t count = 0;

  EnvironmentLayerScope(EnvironmentStack& environment, std::span<EnvironmentLayer const> layers)
      : environment(environment) {
    for (EnvironmentLayer const& layer : layers) {
      environment.push(layer);
      ++count;
    }
  }

  ~EnvironmentLayerScope() {
    while (count-- > 0) {
      environment.pop();
    }
  }
};

LayoutConstraints insetConstraints(LayoutConstraints constraints, EdgeInsets const& padding) {
  float const dx = std::max(0.f, padding.left) + std::max(0.f, padding.right);
  float const dy = std::max(0.f, padding.top) + std::max(0.f, padding.bottom);
  if (std::isfinite(constraints.maxWidth)) {
    constraints.maxWidth = std::max(0.f, constraints.maxWidth - dx);
  }
  if (std::isfinite(constraints.maxHeight)) {
    constraints.maxHeight = std::max(0.f, constraints.maxHeight - dy);
  }
  constraints.minWidth = std::max(0.f, constraints.minWidth - dx);
  constraints.minHeight = std::max(0.f, constraints.minHeight - dy);
  layout::clampLayoutMinToMax(constraints);
  return constraints;
}

Point modifierOffset(detail::ElementModifiers const* mods) {
  if (!mods) {
    return {};
  }
  return Point {mods->positionX + mods->translation.x, mods->positionY + mods->translation.y};
}

bool needsEnvelope(detail::ElementModifiers const* mods, scenegraph::InteractionData const* interaction) {
  if (!mods) {
    return interaction != nullptr;
  }
  return mods->needsModifierPass() || interaction != nullptr;
}

bool contentConsumesBoxPaint(ElementType typeTag) {
  return typeTag == ElementType::Rectangle || typeTag == ElementType::Path;
}

bool compositeKeepsContentGeometry(ElementType typeTag) {
  if (typeTag == ElementType::ScaleAroundCenter) {
    return true;
  }
  switch (typeTag) {
  case ElementType::VStack:
  case ElementType::HStack:
  case ElementType::ZStack:
  case ElementType::Grid:
  case ElementType::OffsetView:
  case ElementType::ScrollView:
    return true;
  default:
    return false;
  }
}

} // namespace

SceneBuilder::SceneBuilder(TextSystem& textSystem, EnvironmentStack& environment,
                           scenegraph::SceneGraph* sceneGraph)
    : textSystem_(textSystem)
    , environment_(environment)
    , sceneGraph_(sceneGraph)
    , measureLayoutCache_(std::make_unique<detail::MeasureLayoutCache>())
    , measureContext_(std::make_unique<MeasureContext>(textSystem_, measureLayoutCache_.get())) {}

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
  detail::MeasureLayoutKey const cacheKey {
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
  MeasureContext& measureContext = *measureContext_;
  measureContext.pushConstraints(constraints, hints);
  measureContext.resetTraversalState(key);
  if (el.expandsBody()) {
    measureContext.setMeasurementRootKey(key);
  } else {
    measureContext.clearMeasurementRootKey();
  }
  measureContext.setCurrentElement(&el);
  Size const measured = el.measure(measureContext, constraints, hints, textSystem_);
  measureContext.setCurrentElement(nullptr);
  measureContext.popConstraints();
  if (measureLayoutCache_) {
    measureLayoutCache_->recordElementSize(cacheKey, measured);
  }
  return measured;
}

std::unique_ptr<scenegraph::SceneNode>
SceneBuilder::wrapModifierLayer(std::unique_ptr<scenegraph::SceneNode> root,
                                detail::ElementModifiers const& layer, ComponentKey const& componentKey,
                                ComponentKey const& interactionKey,
                                LayoutConstraints const& constraints, LayoutHints const& hints,
                                Point origin, Size innerSize, Size outerSize,
                                bool applyBoxPaint,
                                std::unique_ptr<scenegraph::SceneNode> existingWrapper) {
  (void)innerSize;
  std::unique_ptr<scenegraph::RectNode> wrapper{};
  if (existingWrapper && existingWrapper->kind() == scenegraph::SceneNodeKind::Rect) {
    wrapper = std::unique_ptr<scenegraph::RectNode>(
        static_cast<scenegraph::RectNode*>(existingWrapper.release()));
  } else {
    wrapper = std::make_unique<scenegraph::RectNode>();
  }
  Theme const& theme = build::activeTheme(environment_);

  if (applyBoxPaint) {
    wrapper->setFill(build::resolveFillStyle(layer.fill, theme));
    wrapper->setStroke(build::resolveStrokeStyle(layer.stroke, theme));
    wrapper->setShadow(build::resolveShadowStyle(layer.shadow, theme));
    wrapper->setCornerRadius(layer.cornerRadius);
  } else {
    wrapper->setFill(FillStyle::none());
    wrapper->setStroke(StrokeStyle::none());
    wrapper->setShadow(ShadowStyle::none());
    wrapper->setCornerRadius(CornerRadius{});
  }
  wrapper->setClipsContents(layer.clip);
  wrapper->setOpacity(layer.opacity);
  wrapper->setBounds(build::sizeRect(outerSize));

  wrapper->setInteraction(build::makeInteractionData(&layer, interactionKey));
  root->setPosition(Point {std::max(0.f, layer.padding.left), std::max(0.f, layer.padding.top)});

  std::vector<std::unique_ptr<scenegraph::SceneNode>> children{};
  children.push_back(std::move(root));

  if (layer.overlay) {
    LayoutConstraints overlayConstraints = constraints;
    overlayConstraints.minWidth = 0.f;
    overlayConstraints.minHeight = 0.f;
    overlayConstraints.maxWidth =
        outerSize.width > 0.f ? outerSize.width : std::numeric_limits<float>::infinity();
    overlayConstraints.maxHeight =
        outerSize.height > 0.f ? outerSize.height : std::numeric_limits<float>::infinity();
    ComponentKey overlayKey {componentKey, LocalId::fromString("$overlay")};
    pushFrame(overlayConstraints, hints, origin, std::move(overlayKey), outerSize,
              outerSize.width > 0.f, outerSize.height > 0.f);
    std::unique_ptr<scenegraph::SceneNode> overlayNode = buildOrReuse(*layer.overlay, nullptr);
    popFrame();
    if (overlayNode) {
      children.push_back(std::move(overlayNode));
    }
  }

  wrapper->replaceChildren(std::move(children));
  wrapper->setPosition(modifierOffset(&layer));
  return wrapper;
}

std::unique_ptr<scenegraph::SceneNode>
SceneBuilder::build(Element const& el, LayoutConstraints const& constraints, ComponentKey rootKey,
                    bool rootUsesMaxWidthAsAssigned, bool rootUsesMaxHeightAsAssigned) {
  measureLayoutCache_->clear();
  lastBuildStats_ = {};
  if (sceneGraph_) {
    sceneGraph_->beginGeometryBuild();
  }

  Size rootAssigned {};
  bool const hasRootWidth = rootUsesMaxWidthAsAssigned && std::isfinite(constraints.maxWidth);
  bool const hasRootHeight = rootUsesMaxHeightAsAssigned && std::isfinite(constraints.maxHeight);
  if (hasRootWidth) {
    rootAssigned.width = std::max(0.f, constraints.maxWidth);
  }
  if (hasRootHeight) {
    rootAssigned.height = std::max(0.f, constraints.maxHeight);
  }

  pushFrame(constraints, LayoutHints {}, Point {}, std::move(rootKey), rootAssigned, hasRootWidth,
            hasRootHeight);
  std::unique_ptr<scenegraph::SceneNode> node = buildOrReuse(el, nullptr);
  popFrame();

  if (sceneGraph_) {
    if (node) {
      sceneGraph_->finishGeometryBuild();
    } else {
      sceneGraph_->clearGeometry();
    }
  }
  return node;
}

std::unique_ptr<scenegraph::SceneNode>
SceneBuilder::buildSubtree(Element const& el, LayoutConstraints const& constraints,
                           LayoutHints const& hints, Point origin, ComponentKey key,
                           Size assignedSize, bool hasAssignedWidth,
                           bool hasAssignedHeight, Point retainedRootOffset,
                           std::unique_ptr<scenegraph::SceneNode> existing) {
  measureLayoutCache_->clear();
  lastBuildStats_ = {};
  if (sceneGraph_) {
    sceneGraph_->beginGeometryBuild();
  }

  if (existing) {
    existing->setPosition(Point{
        existing->position().x - retainedRootOffset.x,
        existing->position().y - retainedRootOffset.y,
    });
  }

  pushFrame(constraints, hints, origin, std::move(key), assignedSize, hasAssignedWidth,
            hasAssignedHeight);
  std::unique_ptr<scenegraph::SceneNode> node = buildOrReuse(el, std::move(existing));
  popFrame();

  if (node) {
    node->setPosition(Point{
        node->position().x + retainedRootOffset.x,
        node->position().y + retainedRootOffset.y,
    });
  }

  if (sceneGraph_) {
    if (node) {
      sceneGraph_->finishGeometryBuild();
    } else {
      sceneGraph_->clearGeometry();
    }
  }
  return node;
}

std::unique_ptr<scenegraph::SceneNode>
SceneBuilder::buildOrReuse(Element const& el, std::unique_ptr<scenegraph::SceneNode> existing) {
  if (buildFrameDepth_ == 0) {
    return build(el, LayoutConstraints {});
  }

  ++lastBuildStats_.resolvedNodes;
  detail::ResolvedElement resolved = el.resolve(frame().key, frame().constraints);
  return buildResolved(el, resolved, std::move(existing));
}

std::unique_ptr<scenegraph::SceneNode>
SceneBuilder::buildResolved(Element const& el, detail::ResolvedElement const& resolved,
                            std::unique_ptr<scenegraph::SceneNode> existing) {
  auto const current = frame();
  ++lastBuildStats_.materializedNodes;

  if (!resolved.sceneElement) {
    Size const measured = measureElement(el, current.constraints, current.hints, current.key);
    auto root = std::make_unique<scenegraph::GroupNode>(build::sizeRect(measured));
    if (sceneGraph_) {
      sceneGraph_->recordGeometry(current.key,
                                  Rect {current.origin.x, current.origin.y, measured.width,
                                        measured.height});
      sceneGraph_->recordNode(current.key, root.get());
    }
    if (StateStore* store = StateStore::current();
        store && store->findComponentState(current.key)) {
      store->recordSceneElement(current.key, el);
      store->recordBuildSnapshot(current.key, current.constraints, current.hints, current.origin,
                                 current.assignedSize, current.hasAssignedWidth,
                                 current.hasAssignedHeight);
    }
    ++lastBuildStats_.arrangedNodes;
    return root;
  }

  std::vector<std::unique_ptr<scenegraph::SceneNode>> reusableWrappers{};
  while (existing && existing->kind() == scenegraph::SceneNodeKind::Rect &&
         existing->children().size() == 1) {
    std::vector<std::unique_ptr<scenegraph::SceneNode>> children = existing->releaseChildren();
    std::unique_ptr<scenegraph::SceneNode> inner = std::move(children.front());
    reusableWrappers.push_back(std::move(existing));
    existing = std::move(inner);
  }
  auto takeReusableWrapper = [&]() -> std::unique_ptr<scenegraph::SceneNode> {
    if (reusableWrappers.empty()) {
      return nullptr;
    }
    std::unique_ptr<scenegraph::SceneNode> wrapper = std::move(reusableWrappers.back());
    reusableWrappers.pop_back();
    return wrapper;
  };

  Element const& stableSceneEl = *resolved.sceneElement;
  ElementType const typeTag = stableSceneEl.typeTag();
  std::vector<detail::ElementModifiers> const& modifierLayers = resolved.modifierLayers;
  detail::ElementModifiers const* mods = modifierLayers.empty() ? nullptr : &modifierLayers.back();
  EdgeInsets const padding = mods ? mods->padding : EdgeInsets {};
  Point const innerOffset = modifierOffset(mods);

  LayoutConstraints innerConstraints = insetConstraints(current.constraints, padding);
  Size contentAssignedSize = current.assignedSize;
  if (current.hasAssignedWidth) {
    contentAssignedSize.width =
        std::max(0.f, current.assignedSize.width - std::max(0.f, padding.left) -
                           std::max(0.f, padding.right));
  }
  if (current.hasAssignedHeight) {
    contentAssignedSize.height =
        std::max(0.f, current.assignedSize.height - std::max(0.f, padding.top) -
                           std::max(0.f, padding.bottom));
  }
  Point const contentOrigin {
      current.origin.x + innerOffset.x + std::max(0.f, padding.left),
      current.origin.y + innerOffset.y + std::max(0.f, padding.top),
  };

  EnvironmentLayerScope resolvedEnvironment {environment_, resolved.environmentLayers};

  ComponentKey sceneKey = current.key;
  if (resolved.nestSceneUnderFirstBody) {
    sceneKey.push_back(LocalId::fromIndex(0));
  }
  ComponentKey interactionKey =
      resolved.stableInteractionKey.empty() ? current.key : resolved.stableInteractionKey;

  detail::ComponentBuildContext buildContext {*this, traversal_, el, stableSceneEl, typeTag,
                                              std::move(sceneKey), std::move(interactionKey), mods,
                                              modifierLayers.size() > 1, innerConstraints, contentOrigin,
                                              contentAssignedSize};

  detail::ComponentBuildResult built =
      stableSceneEl.buildMeasured(buildContext, std::move(existing));
  std::unique_ptr<scenegraph::SceneNode> root = std::move(built.node);
  if (!root) {
    root = std::make_unique<scenegraph::GroupNode>();
  }

  Size geometrySize = built.hasGeometrySize ? built.geometrySize : build::rectSize(root->bounds());
  Size outerSize {};
  Size layoutOuterSize {};
  buildContext.finalizeOuterSizes(geometrySize, outerSize, layoutOuterSize);

  Point totalOffset {};
  Point geometryOffset = innerOffset;
  if (mods) {
    totalOffset = modifierOffset(mods);
    if (needsEnvelope(mods, root->interaction())) {
      root = wrapModifierLayer(std::move(root), *mods, current.key, buildContext.interactionKey(),
                               current.constraints, current.hints, current.origin + totalOffset,
                               geometrySize, outerSize, !contentConsumesBoxPaint(typeTag),
                               takeReusableWrapper());
    } else {
      if (!root->interaction()) {
        root->setInteraction(build::makeInteractionData(mods, buildContext.interactionKey()));
      }
      root->setPosition(totalOffset);
    }
  }

  Size currentSize = outerSize.width > 0.f || outerSize.height > 0.f ? outerSize : layoutOuterSize;
  if (currentSize.width <= 0.f && currentSize.height <= 0.f) {
    currentSize = build::rectSize(root->bounds());
  }

  for (std::size_t i = modifierLayers.size(); i > 1; --i) {
    detail::ElementModifiers const& outerLayer = modifierLayers[i - 2];
    Point const layerOffset = modifierOffset(&outerLayer);
    totalOffset = Point {totalOffset.x + layerOffset.x, totalOffset.y + layerOffset.y};
    Size const nextOuterSize = build::measuredOuterSizeFromContent(currentSize, &outerLayer);
    if (needsEnvelope(&outerLayer, root->interaction())) {
      root = wrapModifierLayer(std::move(root), outerLayer, current.key,
                               buildContext.interactionKey(), current.constraints, current.hints,
                               current.origin + totalOffset, currentSize,
                               nextOuterSize, true, takeReusableWrapper());
    } else {
      root->setPosition(Point {root->position().x + layerOffset.x,
                               root->position().y + layerOffset.y});
    }
    currentSize = nextOuterSize;
  }

  if (root->bounds().width <= 0.f && root->bounds().height <= 0.f) {
    root->setBounds(build::sizeRect(currentSize));
  }

  if (sceneGraph_) {
    Size const recordedSize =
        geometrySize.width > 0.f || geometrySize.height > 0.f ? geometrySize : build::rectSize(root->bounds());
    Rect const contentRect {
        current.origin.x + geometryOffset.x,
        current.origin.y + geometryOffset.y,
        std::max(0.f, recordedSize.width),
        std::max(0.f, recordedSize.height),
    };
    Rect compositeRect = contentRect;
    Rect logicalCompositeRect = contentRect;
    if (el.expandsBody()) {
      Rect const compositeVisualBounds = scenegraph::detail::subtreeLocalVisualBounds(*root);
      compositeRect = Rect {
          current.origin.x + totalOffset.x,
          current.origin.y + totalOffset.y,
          std::max(0.f, compositeVisualBounds.width),
          std::max(0.f, compositeVisualBounds.height),
      };
      logicalCompositeRect = Rect {
          current.origin.x + totalOffset.x,
          current.origin.y + totalOffset.y,
          std::max(0.f, root->bounds().width),
          std::max(0.f, root->bounds().height),
      };
    }
    bool const usesNestedCompositeBody = !resolved.bodyComponentKeys.empty();
    bool const hasDistinctOuterGeometry =
        !build::nearlyEqual(logicalCompositeRect.x, contentRect.x) ||
        !build::nearlyEqual(logicalCompositeRect.y, contentRect.y) ||
        !build::nearlyEqual(logicalCompositeRect.width, contentRect.width) ||
        !build::nearlyEqual(logicalCompositeRect.height, contentRect.height);
    Rect currentRect = contentRect;
    if (el.expandsBody()) {
      if (usesNestedCompositeBody) {
        currentRect = compositeRect;
      } else if (hasDistinctOuterGeometry || !compositeKeepsContentGeometry(typeTag)) {
        currentRect = logicalCompositeRect;
      }
    }
    sceneGraph_->recordGeometry(current.key, currentRect);
    sceneGraph_->recordNode(current.key, root.get());
    if (resolved.nestSceneUnderFirstBody) {
      ComponentKey bodySceneKey = current.key;
      bodySceneKey.push_back(LocalId::fromIndex(0));
      sceneGraph_->recordGeometry(bodySceneKey, contentRect);
      sceneGraph_->recordNode(bodySceneKey, root.get());
    }
    for (ComponentKey const& bodyKey : resolved.bodyComponentKeys) {
      sceneGraph_->recordGeometry(bodyKey, logicalCompositeRect);
      sceneGraph_->recordNode(bodyKey, root.get());
    }
  }

  if (StateStore* store = StateStore::current();
      store && store->findComponentState(current.key)) {
    store->recordSceneElement(current.key, el);
    store->recordBuildSnapshot(current.key, current.constraints, current.hints, current.origin,
                               current.assignedSize, current.hasAssignedWidth,
                               current.hasAssignedHeight);
  }

  ++lastBuildStats_.arrangedNodes;
  return root;
}

} // namespace flux
