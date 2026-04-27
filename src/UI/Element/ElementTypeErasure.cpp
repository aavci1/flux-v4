#include <Flux/UI/Element.hpp>

#include <Flux/Reactive/Effect.hpp>
#include <Flux/SceneGraph/InteractionData.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>
#include <Flux/UI/MountContext.hpp>

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
                                           float width, float height) {
  float const dx = std::max(0.f, padding.left) + std::max(0.f, padding.right);
  float const dy = std::max(0.f, padding.top) + std::max(0.f, padding.bottom);
  constraints = insetConstraints(constraints, padding);
  if (width > 0.f) {
    float const innerWidth = std::max(0.f, width - dx);
    constraints.maxWidth = innerWidth;
    constraints.minWidth = innerWidth;
  }
  if (height > 0.f) {
    float const innerHeight = std::max(0.f, height - dy);
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

class ScopedEnvironmentSnapshot {
public:
  ScopedEnvironmentSnapshot(EnvironmentStack& stack, std::vector<EnvironmentLayer> const& layers)
      : stack_(stack) {
    for (EnvironmentLayer const& layer : layers) {
      stack_.push(layer);
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

  std::function<void()> requestRedraw = ctx.redrawCallback();
  Reactive::withOwner(ctx.owner(), [&] {
    Reactive::Effect([binding = std::move(binding), setter = std::move(setter),
                       requestRedraw = std::move(requestRedraw), environment,
                       environmentLayers = std::move(environmentLayers)]() mutable {
      ScopedEnvironmentSnapshot environmentScope{*environment, environmentLayers};
      setter(binding.evaluate());
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
      modifierInnerConstraints(ctx.constraints(), padding, width, height);
  MountContext innerCtx = ctx.child(innerConstraints, ctx.hints());
  std::unique_ptr<scenegraph::SceneNode> content = impl_->mount(innerCtx);
  if (!content) {
    popEnvironment();
    return nullptr;
  }

  Size outerSize = measuredOuterSize(*this, ctx);
  auto wrapper = std::make_unique<scenegraph::RectNode>(
      Rect{0.f, 0.f, positive(outerSize.width), positive(outerSize.height)});

  auto* rawWrapper = wrapper.get();
  if (modifiers.hasInteraction()) {
    auto interaction = std::make_unique<scenegraph::InteractionData>();
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

  content->setPosition(Point{std::max(0.f, padding.left), std::max(0.f, padding.top)});
  wrapper->appendChild(std::move(content));

  installBinding<float>(ctx, modifiers.sizeWidth, [rawWrapper](float width) {
    if (width > 0.f) {
      Size size = rawWrapper->size();
      size.width = width;
      rawWrapper->setSize(size);
    }
  });
  installBinding<float>(ctx, modifiers.sizeHeight, [rawWrapper](float height) {
    if (height > 0.f) {
      Size size = rawWrapper->size();
      size.height = height;
      rawWrapper->setSize(size);
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
    MountContext overlayCtx = ctx.child(LayoutConstraints{
        .maxWidth = outerSize.width,
        .maxHeight = outerSize.height,
        .minWidth = 0.f,
        .minHeight = 0.f,
    }, ctx.hints());
    if (auto overlayNode = modifiers.overlay->mount(overlayCtx)) {
      wrapper->appendChild(std::move(overlayNode));
    }
  }

  popEnvironment();
  return wrapper;
}

} // namespace flux
