#include "UI/SceneBuilder/ElementEnvelope.hpp"

#include <Flux/Scene/SceneTree.hpp>
#include <Flux/UI/SceneBuilder.hpp>
#include <Flux/UI/Theme.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <typeinfo>
#include <utility>
#include <vector>

#include "UI/Build/ComponentBuildSupport.hpp"
#include "UI/Layout/LayoutHelpers.hpp"
#include "UI/SceneBuilder/NodeReuse.hpp"

namespace flux {

namespace build_support = ::flux::detail::build;
namespace scene_builder = ::flux::detail::scene_builder;

namespace {

bool isTransparentModifierWrapper(SceneNode const& node) {
  if (node.kind() != SceneNodeKind::Modifier || typeid(node) != typeid(ModifierSceneNode)) {
    return false;
  }
  return node.children().size() == 1 && node.children()[0] && node.children()[0]->id() == node.id();
}

bool isTransparentLayoutWrapper(SceneNode const& node) {
  return build_support::isPlainGroup(node) && !node.children().empty() && node.children()[0] &&
         node.children()[0]->id() == node.id();
}

std::unique_ptr<ModifierSceneNode> takeTransparentModifierWrapper(std::unique_ptr<SceneNode>& node) {
  if (!node || !isTransparentModifierWrapper(*node)) {
    return nullptr;
  }
  auto wrapper = std::unique_ptr<ModifierSceneNode>(static_cast<ModifierSceneNode*>(node.release()));
  std::vector<std::unique_ptr<SceneNode>> children = wrapper->releaseChildren();
  node = children.empty() ? nullptr : std::move(children[0]);
  return wrapper;
}

struct LayoutWrapperReuse {
  std::unique_ptr<SceneNode> wrapper{};
  std::unique_ptr<SceneNode> overlay{};
};

LayoutWrapperReuse takeTransparentLayoutWrapper(std::unique_ptr<SceneNode>& node) {
  if (!node || !isTransparentLayoutWrapper(*node)) {
    return {};
  }

  LayoutWrapperReuse reuse{};
  reuse.wrapper = std::move(node);
  std::vector<std::unique_ptr<SceneNode>> children = reuse.wrapper->releaseChildren();
  node = children.empty() ? nullptr : std::move(children[0]);

  NodeId const overlayId = SceneTree::childId(reuse.wrapper->id(), LocalId::fromString("$overlay"));
  for (std::size_t i = 1; i < children.size(); ++i) {
    if (children[i] && children[i]->id() == overlayId) {
      reuse.overlay = std::move(children[i]);
      break;
    }
  }
  return reuse;
}

bool needsModifierWrapper(detail::ElementModifiers const* mods, bool leafOwnsPaint) {
  if (!mods) {
    return false;
  }
  if (mods->clip || mods->opacity < 1.f - 1e-6f) {
    return true;
  }
  if (leafOwnsPaint) {
    return false;
  }
  return !mods->fill.isNone() || !mods->stroke.isNone() || !mods->shadow.isNone() || !mods->cornerRadius.isZero();
}

bool needsLayoutWrapper(detail::ElementModifiers const* mods, Size outerSize, Size contentSize) {
  if (!mods) {
    return false;
  }
  if (scene_builder::hasPadding(mods) || scene_builder::hasOverlay(mods)) {
    return true;
  }
  return !build_support::nearlyEqual(outerSize, contentSize);
}

Rect chromeRectForSize(Size size) {
  return Rect{0.f, 0.f, std::max(0.f, size.width), std::max(0.f, size.height)};
}

bool rectIsMeaningful(Rect rect) {
  return rect.width > 0.f || rect.height > 0.f;
}

} // namespace

namespace detail::scene_builder {

EnvironmentLayerScope::EnvironmentLayerScope(EnvironmentStack& environment) : environment_(environment) {}

EnvironmentLayerScope::EnvironmentLayerScope(EnvironmentStack& environment,
                                             std::span<EnvironmentLayer const> layers)
    : environment_(environment) {
  for (EnvironmentLayer const& layer : layers) {
    environment_.push(layer);
    ++count_;
  }
}

EnvironmentLayerScope::~EnvironmentLayerScope() {
  while (count_-- > 0) {
    environment_.pop();
  }
}

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
  return Point{mods->positionX + mods->translation.x, mods->positionY + mods->translation.y};
}

bool hasPadding(detail::ElementModifiers const* mods) {
  return mods && !mods->padding.isZero();
}

bool hasOverlay(detail::ElementModifiers const* mods) {
  return mods && mods->overlay != nullptr;
}

bool needsDecorationPass(detail::ElementModifiers const* mods, Size outerSize, Size contentSize,
                         bool leafOwnsPaint, InteractionData const* interaction) {
  if (!mods) {
    return interaction != nullptr;
  }
  if (needsLayoutWrapper(mods, outerSize, contentSize) || needsModifierWrapper(mods, leafOwnsPaint)) {
    return true;
  }
  if (interaction != nullptr) {
    return true;
  }
  Point const offset = modifierOffset(mods);
  return std::fabs(offset.x) > 1e-6f || std::fabs(offset.y) > 1e-6f;
}

DecorationReuse takeDecorationReuse(std::unique_ptr<SceneNode>& node, detail::ElementModifiers const* mods,
                                    bool leafOwnsModifierPaint, bool allowLayoutWrapperReuse) {
  DecorationReuse reuse{};
  if (!mods) {
    return reuse;
  }
  if (needsModifierWrapper(mods, leafOwnsModifierPaint)) {
    reuse.modifierWrapper = takeTransparentModifierWrapper(node);
  }
  if (allowLayoutWrapperReuse) {
    LayoutWrapperReuse layoutReuse = takeTransparentLayoutWrapper(node);
    reuse.layoutWrapper = std::move(layoutReuse.wrapper);
    reuse.overlay = std::move(layoutReuse.overlay);
  }
  return reuse;
}

} // namespace detail::scene_builder

std::unique_ptr<SceneNode>
SceneBuilder::decorateNode(std::unique_ptr<SceneNode> root, bool leafOwnsModifierPaint,
                           detail::ElementModifiers const* mods,
                           std::unique_ptr<ModifierSceneNode> existingModifierWrapper,
                           std::unique_ptr<SceneNode> existingLayoutWrapper,
                           std::unique_ptr<SceneNode> existingOverlay, Size layoutOuterSize, Size outerSize,
                           Point subtreeOffset, EdgeInsets const& padding,
                           std::unique_ptr<InteractionData> interaction) {
  if (!root) {
    return nullptr;
  }

  auto const current = frame();
  Size const contentSize = build_support::rectSize(root->bounds);
  root->position = {};

  if (scene_builder::hasPadding(mods) || scene_builder::hasOverlay(mods) ||
      !build_support::nearlyEqual(layoutOuterSize, contentSize)) {
    std::unique_ptr<SceneNode> wrapper = build_support::releasePlainGroup(std::move(existingLayoutWrapper));
    if (!wrapper) {
      wrapper = std::make_unique<SceneNode>(root->id());
    }
    std::vector<std::unique_ptr<SceneNode>> children{};
    children.reserve(scene_builder::hasOverlay(mods) ? 2 : 1);
    root->position = Point{std::max(0.f, padding.left), std::max(0.f, padding.top)};
    children.push_back(std::move(root));
    if (scene_builder::hasOverlay(mods)) {
      LayoutConstraints overlayConstraints = current.constraints;
      overlayConstraints.maxWidth =
          outerSize.width > 0.f ? outerSize.width : std::numeric_limits<float>::infinity();
      overlayConstraints.maxHeight =
          outerSize.height > 0.f ? outerSize.height : std::numeric_limits<float>::infinity();
      ComponentKey overlayKey = current.key;
      overlayKey.push_back(LocalId::fromString("$overlay"));
      pushFrame(overlayConstraints, current.hints,
                Point{current.origin.x + subtreeOffset.x, current.origin.y + subtreeOffset.y},
                std::move(overlayKey), layoutOuterSize, layoutOuterSize.width > 0.f,
                layoutOuterSize.height > 0.f);
      std::unique_ptr<SceneNode> overlayNode =
          buildOrReuse(*mods->overlay, SceneTree::childId(wrapper->id(), LocalId::fromString("$overlay")),
                       std::move(existingOverlay));
      popFrame();
      children.push_back(std::move(overlayNode));
    }
    wrapper->replaceChildren(std::move(children));
    build_support::setAssignedGroupBounds(*wrapper, layoutOuterSize);
    root = std::move(wrapper);
  }

  if (needsModifierWrapper(mods, leafOwnsModifierPaint)) {
    std::unique_ptr<ModifierSceneNode> wrapper = std::move(existingModifierWrapper);
    if (!wrapper) {
      wrapper = std::make_unique<ModifierSceneNode>(root->id());
    }
    wrapper->replaceChildren({});
    root->position = {};
    wrapper->appendChild(std::move(root));

    std::optional<Rect> const nextClip =
        mods->clip ? std::optional<Rect>(chromeRectForSize(outerSize)) : std::nullopt;
    Theme const& theme = build_support::activeTheme(environment_);
    FillStyle const nextFill =
        leafOwnsModifierPaint ? FillStyle::none() : build_support::resolveFillStyle(mods->fill, theme);
    StrokeStyle const nextStroke =
        leafOwnsModifierPaint ? StrokeStyle::none() : build_support::resolveStrokeStyle(mods->stroke, theme);
    ShadowStyle const nextShadow =
        leafOwnsModifierPaint ? ShadowStyle::none() : build_support::resolveShadowStyle(mods->shadow, theme);
    CornerRadius const nextCornerRadius = leafOwnsModifierPaint ? CornerRadius{} : mods->cornerRadius;
    Rect const nextChromeRect =
        !leafOwnsModifierPaint && rectIsMeaningful(chromeRectForSize(layoutOuterSize))
            ? chromeRectForSize(layoutOuterSize)
            : Rect{};

    bool const paintChanged = wrapper->chromeRect != nextChromeRect || wrapper->fill != nextFill ||
                              wrapper->stroke != nextStroke || wrapper->shadow != nextShadow ||
                              wrapper->cornerRadius != nextCornerRadius;
    bool const boundsChanged =
        wrapper->clip != nextClip || wrapper->opacity != mods->opacity || paintChanged;

    wrapper->chromeRect = nextChromeRect;
    wrapper->clip = nextClip;
    wrapper->opacity = mods->opacity;
    wrapper->fill = nextFill;
    wrapper->stroke = nextStroke;
    wrapper->shadow = nextShadow;
    wrapper->cornerRadius = nextCornerRadius;
    if (paintChanged) {
      wrapper->invalidatePaint();
    }
    if (boundsChanged) {
      wrapper->markBoundsDirty();
    }
    wrapper->recomputeBounds();
    root = std::move(wrapper);
  }

  root->setInteraction(std::move(interaction));
  root->position = subtreeOffset;
  return root;
}

} // namespace flux
