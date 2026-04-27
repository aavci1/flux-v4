#include <Flux/UI/Views/ScaleAroundCenter.hpp>

#include <Flux/Reactive/Effect.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/UI/MeasureContext.hpp>

#include "SceneGraph/SceneBounds.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

namespace flux {

namespace {

LayoutConstraints fixedConstraints(Size size) {
  return LayoutConstraints{
      .maxWidth = std::max(0.f, size.width),
      .maxHeight = std::max(0.f, size.height),
      .minWidth = std::max(0.f, size.width),
      .minHeight = std::max(0.f, size.height),
  };
}

void includeVisualBounds(scenegraph::GroupNode& group) {
  Rect const visual = scenegraph::detail::subtreeLocalVisualBounds(group);
  Size size = group.size();
  size.width = std::max(size.width, visual.width);
  size.height = std::max(size.height, visual.height);
  group.setSize(size);
}

} // namespace

Size ScaleAroundCenter::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                                LayoutHints const& hints, TextSystem& textSystem) const {
  ctx.pushConstraints(constraints, hints);
  Size size = child.measure(ctx, constraints, hints, textSystem);
  ctx.popConstraints();
  return size;
}

std::unique_ptr<scenegraph::SceneNode> ScaleAroundCenter::mount(MountContext& ctx) const {
  Size const measured = measure(ctx.measureContext(), ctx.constraints(), ctx.hints(), ctx.textSystem());
  auto group = std::make_unique<scenegraph::GroupNode>(
      Rect{0.f, 0.f, std::max(0.f, measured.width), std::max(0.f, measured.height)});

  MountContext childCtx = ctx.child(fixedConstraints(measured), ctx.hints());
  auto childNode = child.mount(childCtx);
  if (!childNode) {
    return group;
  }

  auto* rawChild = childNode.get();
  auto frameSize = std::make_shared<Size>(measured);
  Reactive::Bindable<float> scaleBinding = scale;
  std::function<void()> requestRedraw = ctx.redrawCallback();
  auto applyScale = [rawChild, frameSize](float value) {
    Point const pivot{frameSize->width * 0.5f, frameSize->height * 0.5f};
    rawChild->setTransform(Mat3::translate(pivot) * Mat3::scale(value) *
                           Mat3::translate(Point{-pivot.x, -pivot.y}));
  };
  applyScale(scaleBinding.evaluate());
  if (scaleBinding.isReactive()) {
    Reactive::Bindable<float> effectScaleBinding = scaleBinding;
    Reactive::withOwner(ctx.owner(), [scaleBinding = std::move(effectScaleBinding), applyScale,
                                      requestRedraw = std::move(requestRedraw)]() mutable {
      Reactive::Effect([scaleBinding, applyScale, requestRedraw]() mutable {
        applyScale(scaleBinding.evaluate());
        if (requestRedraw) {
          requestRedraw();
        }
      });
    });
  }

  group->appendChild(std::move(childNode));
  includeVisualBounds(*group);
  auto* rawGroup = group.get();
  rawGroup->setRelayout([rawGroup, rawChild, frameSize, applyScale, scaleBinding](
                            LayoutConstraints const& constraints) mutable {
    if (rawChild) {
      rawChild->relayout(constraints);
      *frameSize = rawChild->size();
      rawChild->relayout(fixedConstraints(*frameSize));
      rawChild->setPosition(Point{});
    }
    rawGroup->setSize(*frameSize);
    applyScale(scaleBinding.evaluate());
    includeVisualBounds(*rawGroup);
  });
  return group;
}

} // namespace flux
