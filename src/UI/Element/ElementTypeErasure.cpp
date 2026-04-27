#include <Flux/UI/Element.hpp>

#include <Flux/Reactive/Effect.hpp>
#include <Flux/SceneGraph/InteractionData.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/MountContext.hpp>

#include "UI/Element/ModifierLayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace flux {

namespace {

float positive(float value) {
  return std::isfinite(value) ? std::max(0.f, value) : 0.f;
}

LayoutConstraints insetConstraints(LayoutConstraints constraints, EdgeInsets padding) {
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
  if (std::isfinite(constraints.maxWidth)) {
    constraints.minWidth = std::min(constraints.minWidth, constraints.maxWidth);
  }
  if (std::isfinite(constraints.maxHeight)) {
    constraints.minHeight = std::min(constraints.minHeight, constraints.maxHeight);
  }
  return constraints;
}

LayoutConstraints modifierInnerConstraints(LayoutConstraints constraints, EdgeInsets padding,
                                           LayoutHints const& hints, float width, float height) {
  float const dx = std::max(0.f, padding.left) + std::max(0.f, padding.right);
  float const dy = std::max(0.f, padding.top) + std::max(0.f, padding.bottom);
  float const resolvedWidth = detail::resolvedModifierWidth(constraints, hints, width);
  float const resolvedHeight = detail::resolvedModifierHeight(constraints, hints, height);
  constraints = insetConstraints(constraints, padding);
  if (resolvedWidth > 0.f) {
    float const innerWidth = std::max(0.f, resolvedWidth - dx);
    constraints.maxWidth = innerWidth;
    constraints.minWidth = innerWidth;
  }
  if (resolvedHeight > 0.f) {
    float const innerHeight = std::max(0.f, resolvedHeight - dy);
    constraints.maxHeight = innerHeight;
    constraints.minHeight = innerHeight;
  }
  if (std::isfinite(constraints.maxWidth)) {
    constraints.minWidth = std::min(constraints.minWidth, constraints.maxWidth);
  }
  if (std::isfinite(constraints.maxHeight)) {
    constraints.minHeight = std::min(constraints.minHeight, constraints.maxHeight);
  }
  return constraints;
}

Size resolveModifierOuterSize(Size size, LayoutConstraints const& constraints,
                              LayoutHints const& hints, float width, float height) {
  float const resolvedWidth = detail::resolvedModifierWidth(constraints, hints, width);
  float const resolvedHeight = detail::resolvedModifierHeight(constraints, hints, height);
  if (resolvedWidth > 0.f) {
    size.width = resolvedWidth;
  }
  if (resolvedHeight > 0.f) {
    size.height = resolvedHeight;
  }
  return Size{positive(size.width), positive(size.height)};
}

LayoutConstraints fixedConstraints(Size size) {
  return LayoutConstraints{
      .maxWidth = std::max(0.f, size.width),
      .maxHeight = std::max(0.f, size.height),
      .minWidth = std::max(0.f, size.width),
      .minHeight = std::max(0.f, size.height),
  };
}

void relayoutStoredAncestors(scenegraph::SceneNode& node) {
  constexpr float epsilon = 0.01f;
  scenegraph::SceneNode* current = &node;
  for (int depth = 0; depth < 64; ++depth) {
    scenegraph::SceneNode* parent = current->parent();
    if (!parent) {
      return;
    }
    Size const oldSize = parent->size();
    if (!parent->relayoutStoredConstraints()) {
      return;
    }
    Size const newSize = parent->size();
    if (std::abs(newSize.width - oldSize.width) <= epsilon &&
        std::abs(newSize.height - oldSize.height) <= epsilon) {
      return;
    }
    current = parent;
  }
}

class ScopedEnvironmentSnapshot {
public:
  ScopedEnvironmentSnapshot(EnvironmentStack& stack, std::vector<EnvironmentLayer> const& layers)
      : stack_(stack) {
    for (EnvironmentLayer const& layer : layers) {
      stack_.pushBorrowed(layer);
      ++pushed_;
    }
  }

  ScopedEnvironmentSnapshot(ScopedEnvironmentSnapshot const&) = delete;
  ScopedEnvironmentSnapshot& operator=(ScopedEnvironmentSnapshot const&) = delete;

  ~ScopedEnvironmentSnapshot() {
    while (pushed_ > 0) {
      stack_.pop();
      --pushed_;
    }
  }

private:
  EnvironmentStack& stack_;
  std::size_t pushed_ = 0;
};

template<typename T, typename Setter>
void installBinding(MountContext& ctx, Reactive::Bindable<T> binding, Setter setter) {
  EnvironmentStack* environment = &ctx.environment();
  std::vector<EnvironmentLayer> environmentLayers = environment->snapshot();
  if (!binding.isReactive()) {
    ScopedEnvironmentSnapshot environmentScope{*environment, environmentLayers};
    setter(binding.evaluate());
    return;
  }

  Reactive::SmallFn<void()> requestRedraw = ctx.redrawCallback();
  bool const initiallyTracksEnvironment = binding.tracksEnvironment();
  Reactive::withOwner(ctx.owner(), [&] {
    Reactive::Effect([binding = std::move(binding), setter = std::move(setter),
                       requestRedraw = std::move(requestRedraw), environment,
                       environmentLayers = std::move(environmentLayers),
                       firstRun = true,
                       tracksEnvironment = initiallyTracksEnvironment]() mutable {
      auto evaluateWithEnvironment = [&] {
        ScopedEnvironmentSnapshot environmentScope{*environment, environmentLayers};
        detail::EnvironmentReadTrackingScope environmentReads;
        setter(binding.evaluate());
        if (environmentReads.observed()) {
          tracksEnvironment = true;
          binding.setTracksEnvironment(true);
        }
      };

      if (tracksEnvironment || firstRun) {
        evaluateWithEnvironment();
        firstRun = false;
      } else {
        detail::EnvironmentReadTrackingScope environmentReads;
        setter(binding.evaluate());
        if (environmentReads.observed()) {
          tracksEnvironment = true;
          binding.setTracksEnvironment(true);
          evaluateWithEnvironment();
        }
      }
      if (requestRedraw) {
        requestRedraw();
      }
    });
  });
}

Size measuredOuterSize(Element const& element, MountContext& ctx) {
  ctx.measureContext().pushConstraints(ctx.constraints(), ctx.hints());
  Size size = element.measure(ctx.measureContext(), ctx.constraints(), ctx.hints(), ctx.textSystem());
  ctx.measureContext().popConstraints();
  return Size{positive(size.width), positive(size.height)};
}

} // namespace

namespace detail {

void ElementDeleter::operator()(Element* element) const noexcept {
  delete element;
}

std::uint64_t nextElementMeasureId() {
  static std::uint64_t next = 1;
  return next++;
}

Popover* popoverOverlayStateIf(Element& el) {
  (void)el;
  return nullptr;
}

} // namespace detail

Element::Element(Element const& other) = default;

Element& Element::operator=(Element const& other) = default;

void Element::ensureUniqueImpl() {
  if (impl_ && impl_.use_count() != 1) {
    impl_ = std::shared_ptr<Concept>(impl_->clone());
  }
}

detail::ElementModifiers& Element::writableModifiers() {
  if (!modifiers_) {
    modifiers_ = std::make_shared<detail::ElementModifiers>();
  } else if (modifiers_.use_count() != 1) {
    modifiers_ = std::make_shared<detail::ElementModifiers>(*modifiers_);
  }
  return *modifiers_;
}

float Element::flexGrow() const {
  return flexGrowOverride_.value_or(impl_->flexGrow());
}

float Element::flexShrink() const {
  return flexShrinkOverride_.value_or(impl_->flexShrink());
}

std::optional<float> Element::flexBasis() const {
  if (flexBasisOverride_.has_value()) {
    return flexBasisOverride_;
  }
  return impl_->flexBasis();
}

bool Element::mountsWhenCollapsed() const {
  return impl_ && impl_->mountsWhenCollapsed();
}

float Element::minMainSize() const {
  return minMainSizeOverride_.value_or(impl_->minMainSize());
}

Element Element::flex(float grow) && {
  flexGrowOverride_ = grow;
  flexShrinkOverride_ = 1.f;
  flexBasisOverride_.reset();
  minMainSizeOverride_.reset();
  return std::move(*this);
}

Element Element::flex(float grow, float shrink) && {
  flexGrowOverride_ = grow;
  flexShrinkOverride_ = shrink;
  flexBasisOverride_.reset();
  minMainSizeOverride_.reset();
  return std::move(*this);
}

Element Element::flex(float grow, float shrink, float basis) && {
  flexGrowOverride_ = grow;
  flexShrinkOverride_ = shrink;
  flexBasisOverride_ = std::max(0.f, basis);
  minMainSizeOverride_.reset();
  return std::move(*this);
}

Element Element::key(std::string key) && {
  key_ = std::move(key);
  return std::move(*this);
}

std::unique_ptr<scenegraph::SceneNode> Element::mount(MountContext& ctx) const {
  if (envLayer_) {
    ctx.environment().push(*envLayer_);
  }

  auto popEnvironment = [&] {
    if (envLayer_) {
      ctx.environment().pop();
    }
  };

  if (!modifiers_ || !modifiers_->needsModifierPass()) {
    auto node = impl_->mount(ctx);
    popEnvironment();
    return node;
  }

  detail::ElementModifiers const& modifiers = *modifiers_;
  EdgeInsets const padding = modifiers.padding.evaluate();
  float const width = modifiers.sizeWidth.evaluate();
  float const height = modifiers.sizeHeight.evaluate();
  LayoutConstraints innerConstraints =
      modifierInnerConstraints(ctx.constraints(), padding, ctx.hints(), width, height);
  MountContext innerCtx = ctx.childWithSharedScope(innerConstraints, ctx.hints());
  std::unique_ptr<scenegraph::SceneNode> content = impl_->mount(innerCtx);
  if (!content) {
    popEnvironment();
    return nullptr;
  }

  Size outerSize = resolveModifierOuterSize(
      measuredOuterSize(*this, ctx), ctx.constraints(), ctx.hints(), width, height);
  auto wrapper = std::make_unique<scenegraph::RectNode>(
      Rect{0.f, 0.f, positive(outerSize.width), positive(outerSize.height)});

  auto* rawWrapper = wrapper.get();
  if (modifiers.hasInteraction()) {
    auto interaction = std::make_unique<scenegraph::InteractionData>();
    interaction->stableTargetKey = ctx.measureContext().currentElementKey();
    interaction->onTap = modifiers.onTap;
    interaction->onPointerEnter = modifiers.onPointerEnter;
    interaction->onPointerExit = modifiers.onPointerExit;
    interaction->onFocus = modifiers.onFocus;
    interaction->onBlur = modifiers.onBlur;
    interaction->onPointerDown = modifiers.onPointerDown;
    interaction->onPointerUp = modifiers.onPointerUp;
    interaction->onPointerMove = modifiers.onPointerMove;
    interaction->onScroll = modifiers.onScroll;
    interaction->onKeyDown = modifiers.onKeyDown;
    interaction->onKeyUp = modifiers.onKeyUp;
    interaction->onTextInput = modifiers.onTextInput;
    interaction->focusable = modifiers.focusable;
    interaction->cursor = modifiers.cursor;
    if (detail::InteractionSignalBundle const* signals = detail::currentInteractionSignals()) {
      interaction->hoverSignal = signals->hover;
      interaction->pressSignal = signals->press;
      interaction->focusSignal = signals->focus;
      interaction->keyboardFocusSignal = signals->keyboardFocus;
    }
    rawWrapper->setInteraction(std::move(interaction));
  }
  installBinding<FillStyle>(ctx, modifiers.fill, [rawWrapper](FillStyle fill) {
    rawWrapper->setFill(std::move(fill));
  });
  installBinding<StrokeStyle>(ctx, modifiers.stroke, [rawWrapper](StrokeStyle stroke) {
    rawWrapper->setStroke(std::move(stroke));
  });
  installBinding<ShadowStyle>(ctx, modifiers.shadow, [rawWrapper](ShadowStyle shadow) {
    rawWrapper->setShadow(shadow);
  });
  installBinding<CornerRadius>(ctx, modifiers.cornerRadius, [rawWrapper](CornerRadius radius) {
    rawWrapper->setCornerRadius(radius);
  });
  installBinding<float>(ctx, modifiers.opacity, [rawWrapper](float opacity) {
    rawWrapper->setOpacity(opacity);
  });
  rawWrapper->setClipsContents(modifiers.clip);

  scenegraph::SceneNode* rawContent = content.get();
  content->setPosition(Point{std::max(0.f, padding.left), std::max(0.f, padding.top)});
  wrapper->appendChild(std::move(content));

  LayoutConstraints const bindingConstraints = ctx.constraints();
  LayoutHints const bindingHints = ctx.hints();
  installBinding<float>(ctx, modifiers.sizeWidth,
                        [rawWrapper, bindingConstraints, bindingHints](float width) {
    float const resolvedWidth = detail::resolvedModifierWidth(bindingConstraints, bindingHints, width);
    if (resolvedWidth > 0.f) {
      Size size = rawWrapper->size();
      Size const oldSize = size;
      size.width = resolvedWidth;
      rawWrapper->setSize(size);
      if (rawWrapper->size() != oldSize) {
        relayoutStoredAncestors(*rawWrapper);
      }
    }
  });
  installBinding<float>(ctx, modifiers.sizeHeight,
                        [rawWrapper, bindingConstraints, bindingHints](float height) {
    float const resolvedHeight = detail::resolvedModifierHeight(bindingConstraints, bindingHints, height);
    if (resolvedHeight > 0.f) {
      Size size = rawWrapper->size();
      Size const oldSize = size;
      size.height = resolvedHeight;
      rawWrapper->setSize(size);
      if (rawWrapper->size() != oldSize) {
        relayoutStoredAncestors(*rawWrapper);
      }
    }
  });
  installBinding<Vec2>(ctx, modifiers.translation, [rawWrapper, px = modifiers.positionX,
                                                    py = modifiers.positionY](Vec2 translation) mutable {
    rawWrapper->setPosition(Point{px.evaluate() + translation.x, py.evaluate() + translation.y});
  });
  installBinding<float>(ctx, modifiers.positionX, [rawWrapper, tr = modifiers.translation,
                                                   py = modifiers.positionY](float x) mutable {
    Vec2 const translation = tr.evaluate();
    rawWrapper->setPosition(Point{x + translation.x, py.evaluate() + translation.y});
  });
  installBinding<float>(ctx, modifiers.positionY, [rawWrapper, tr = modifiers.translation,
                                                   px = modifiers.positionX](float y) mutable {
    Vec2 const translation = tr.evaluate();
    rawWrapper->setPosition(Point{px.evaluate() + translation.x, y + translation.y});
  });

  if (modifiers.overlay) {
    MountContext overlayCtx = ctx.childWithSharedScope(LayoutConstraints{
        .maxWidth = outerSize.width,
        .maxHeight = outerSize.height,
        .minWidth = 0.f,
        .minHeight = 0.f,
    }, ctx.hints());
    if (auto overlayNode = modifiers.overlay->mount(overlayCtx)) {
      wrapper->appendChild(std::move(overlayNode));
    }
  }

  LayoutHints const relayoutHints = ctx.hints();
  rawWrapper->setLayoutConstraints(ctx.constraints());
  rawWrapper->setRelayout([rawWrapper, rawContent, relayoutHints,
                           modifiers](LayoutConstraints const& constraints) mutable {
    EdgeInsets const padding = modifiers.padding.evaluate();
    float const padL = std::max(0.f, padding.left);
    float const padR = std::max(0.f, padding.right);
    float const padT = std::max(0.f, padding.top);
    float const padB = std::max(0.f, padding.bottom);
    float const width = modifiers.sizeWidth.evaluate();
    float const height = modifiers.sizeHeight.evaluate();
    float const resolvedWidth = detail::resolvedModifierWidth(constraints, relayoutHints, width);
    float const resolvedHeight = detail::resolvedModifierHeight(constraints, relayoutHints, height);
    LayoutConstraints innerConstraints =
        modifierInnerConstraints(constraints, padding, relayoutHints, width, height);
    if (rawContent) {
      rawContent->relayout(innerConstraints);
    }
    Size contentSize = rawContent ? rawContent->size() : Size{};
    Size nextSize{contentSize.width + padL + padR, contentSize.height + padT + padB};
    if (resolvedWidth > 0.f) {
      nextSize.width = resolvedWidth;
    }
    if (resolvedHeight > 0.f) {
      nextSize.height = resolvedHeight;
    }
    if (resolvedWidth <= 0.f) {
      nextSize.width = std::max(nextSize.width, constraints.minWidth);
    }
    if (resolvedHeight <= 0.f) {
      nextSize.height = std::max(nextSize.height, constraints.minHeight);
    }
    if (resolvedWidth <= 0.f && std::isfinite(constraints.maxWidth)) {
      nextSize.width = std::min(nextSize.width, constraints.maxWidth);
    }
    if (resolvedHeight <= 0.f && std::isfinite(constraints.maxHeight)) {
      nextSize.height = std::min(nextSize.height, constraints.maxHeight);
    }
    rawWrapper->setSize(Size{positive(nextSize.width), positive(nextSize.height)});
    if (rawContent) {
      rawContent->setPosition(Point{padL, padT});
    }
    auto children = rawWrapper->children();
    for (std::size_t i = 1; i < children.size(); ++i) {
      if (children[i]) {
        children[i]->relayout(fixedConstraints(rawWrapper->size()));
      }
    }
  });

  popEnvironment();
  return wrapper;
}

} // namespace flux
