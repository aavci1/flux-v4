#include <Flux/UI/Element.hpp>

#include <Flux/Reactive/Effect.hpp>
#include <Flux/SceneGraph/InteractionData.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/MountContext.hpp>
#include <Flux/UI/Theme.hpp>

#include "UI/Element/ModifierLayoutHelpers.hpp"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cmath>
#include <limits>
#include <memory>
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

Theme const& activeTheme(EnvironmentStack& environment) {
  if (auto const* themeSignal = environment.findSignal<Theme>()) {
    return themeSignal->get();
  }
  if (Theme const* theme = environment.find<Theme>()) {
    return *theme;
  }
  static Theme const fallback = Theme::light();
  return fallback;
}

FillStyle resolveFillStyle(FillStyle style, Theme const& theme) {
  Color color{};
  if (!style.solidColor(&color)) {
    return style;
  }
  style.data = resolveColor(color, theme);
  return style;
}

StrokeStyle resolveStrokeStyle(StrokeStyle style, Theme const& theme) {
  if (style.type == StrokeStyle::Type::Solid) {
    style.color = resolveColor(style.color, theme);
  }
  return style;
}

ShadowStyle resolveShadowStyle(ShadowStyle style, Theme const& theme) {
  style.color = resolveColor(style.color, theme);
  return style;
}

void relayoutStoredAncestors(scenegraph::SceneNode& node) {
  constexpr float epsilon = 0.01f;
  // Trees deeper than 64 stored scene-graph ancestors are not supported for reactive
  // relayout propagation. This bounds the synchronous relayout walk; current demos are
  // far shallower (lambda-studio max observed retained depth is 16).
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
  assert(false && "reactive relayout ancestor walk exceeded the 64-level depth cap");
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
  MountContext::EnvironmentSnapshot environmentSnapshot = ctx.environmentSnapshot();
  if (!binding.isReactive()) {
    ScopedEnvironmentSnapshot environmentScope{*environment, *environmentSnapshot};
    setter(binding.evaluate());
    return;
  }

  Reactive::SmallFn<void()> requestRedraw = ctx.redrawCallback();
  bool const initiallyTracksEnvironment = binding.tracksEnvironment();
  Reactive::withOwner(ctx.owner(), [&] {
    Reactive::Effect([binding = std::move(binding), setter = std::move(setter),
                       requestRedraw = std::move(requestRedraw), environment,
                       environmentSnapshot = std::move(environmentSnapshot),
                       lastValue = std::optional<T>{},
                       firstRun = true,
                       tracksEnvironment = initiallyTracksEnvironment]() mutable {
      auto applyValue = [&](T value) {
        if constexpr (std::equality_comparable<T>) {
          if (lastValue && *lastValue == value) {
            return false;
          }
          lastValue = value;
        }
        setter(std::move(value));
        return true;
      };

      auto evaluateWithEnvironment = [&] {
        ScopedEnvironmentSnapshot environmentScope{*environment, *environmentSnapshot};
        detail::EnvironmentReadTrackingScope environmentReads;
        bool const changed = applyValue(binding.evaluate());
        if (environmentReads.observed()) {
          tracksEnvironment = true;
          binding.setTracksEnvironment(true);
        }
        return changed;
      };

      bool changed = false;
      if (tracksEnvironment || firstRun) {
        changed = evaluateWithEnvironment();
        firstRun = false;
      } else {
        detail::EnvironmentReadTrackingScope environmentReads;
        changed = applyValue(binding.evaluate());
        if (environmentReads.observed()) {
          tracksEnvironment = true;
          binding.setTracksEnvironment(true);
          changed = evaluateWithEnvironment() || changed;
        }
      }
      if (changed && requestRedraw) {
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
  std::unique_ptr<MountContext> scopedEnvironmentContext;
  if (envLayer_) {
    ctx.environment().push(*envLayer_);
    auto environmentSnapshot =
        std::make_shared<std::vector<EnvironmentLayer> const>(ctx.environment().snapshot());
    scopedEnvironmentContext = std::make_unique<MountContext>(
        ctx.owner(), ctx.environment(), ctx.textSystem(), ctx.measureContext(),
        ctx.constraints(), ctx.hints(), ctx.redrawCallback(), std::move(environmentSnapshot));
  }
  MountContext& activeCtx = scopedEnvironmentContext ? *scopedEnvironmentContext : ctx;

  auto popEnvironment = [&] {
    if (envLayer_) {
      ctx.environment().pop();
    }
  };

  if (!modifiers_ || !modifiers_->needsModifierPass()) {
    auto node = impl_->mount(activeCtx);
    popEnvironment();
    return node;
  }

  detail::ElementModifiers const& modifiers = *modifiers_;
  EdgeInsets const padding = modifiers.padding.evaluate();
  float const width = modifiers.sizeWidth.evaluate();
  float const height = modifiers.sizeHeight.evaluate();
  LayoutConstraints innerConstraints =
      modifierInnerConstraints(activeCtx.constraints(), padding, activeCtx.hints(), width, height);
  MountContext innerCtx = activeCtx.childWithSharedScope(innerConstraints, activeCtx.hints());
  std::unique_ptr<scenegraph::SceneNode> content = impl_->mount(innerCtx);
  if (!content) {
    popEnvironment();
    return nullptr;
  }

  Size outerSize = resolveModifierOuterSize(
      measuredOuterSize(*this, activeCtx), activeCtx.constraints(), activeCtx.hints(), width, height);
  auto wrapper = std::make_unique<scenegraph::RectNode>(
      Rect{0.f, 0.f, positive(outerSize.width), positive(outerSize.height)});

  auto* rawWrapper = wrapper.get();
  if (modifiers.hasInteraction()) {
    auto interaction = std::make_unique<scenegraph::InteractionData>();
    if (ComponentKey const* scopeKey = detail::currentInteractionScopeKey()) {
      ComponentKey targetKey = *scopeKey;
      for (LocalId const id : ctx.measureContext().currentElementKey().materialize()) {
        targetKey.push_back(id);
      }
      interaction->stableTargetKey = std::move(targetKey);
    } else {
      interaction->stableTargetKey = ctx.measureContext().currentElementKey();
    }
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
  EnvironmentStack* bindingEnvironment = &activeCtx.environment();
  installBinding<FillStyle>(activeCtx, modifiers.fill, [rawWrapper, bindingEnvironment](FillStyle fill) {
    rawWrapper->setFill(resolveFillStyle(std::move(fill), activeTheme(*bindingEnvironment)));
  });
  installBinding<StrokeStyle>(activeCtx, modifiers.stroke, [rawWrapper, bindingEnvironment](StrokeStyle stroke) {
    rawWrapper->setStroke(resolveStrokeStyle(std::move(stroke), activeTheme(*bindingEnvironment)));
  });
  installBinding<ShadowStyle>(activeCtx, modifiers.shadow, [rawWrapper, bindingEnvironment](ShadowStyle shadow) {
    rawWrapper->setShadow(resolveShadowStyle(shadow, activeTheme(*bindingEnvironment)));
  });
  installBinding<CornerRadius>(activeCtx, modifiers.cornerRadius, [rawWrapper](CornerRadius radius) {
    rawWrapper->setCornerRadius(radius);
  });
  installBinding<float>(activeCtx, modifiers.opacity, [rawWrapper](float opacity) {
    rawWrapper->setOpacity(opacity);
  });
  rawWrapper->setClipsContents(modifiers.clip);

  scenegraph::SceneNode* rawContent = content.get();
  content->setPosition(Point{std::max(0.f, padding.left), std::max(0.f, padding.top)});
  wrapper->appendChild(std::move(content));

  LayoutConstraints const bindingConstraints = activeCtx.constraints();
  LayoutHints const bindingHints = activeCtx.hints();
  installBinding<float>(activeCtx, modifiers.sizeWidth,
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
  installBinding<float>(activeCtx, modifiers.sizeHeight,
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
  installBinding<Vec2>(activeCtx, modifiers.translation, [rawWrapper, px = modifiers.positionX,
                                                          py = modifiers.positionY](
                                                           Vec2 translation) mutable {
    rawWrapper->setPosition(Point{px.evaluate() + translation.x, py.evaluate() + translation.y});
  });
  installBinding<float>(activeCtx, modifiers.positionX, [rawWrapper, tr = modifiers.translation,
                                                         py = modifiers.positionY](float x) mutable {
    Vec2 const translation = tr.evaluate();
    rawWrapper->setPosition(Point{x + translation.x, py.evaluate() + translation.y});
  });
  installBinding<float>(activeCtx, modifiers.positionY, [rawWrapper, tr = modifiers.translation,
                                                         px = modifiers.positionX](float y) mutable {
    Vec2 const translation = tr.evaluate();
    rawWrapper->setPosition(Point{px.evaluate() + translation.x, y + translation.y});
  });

  if (modifiers.overlay) {
    MountContext overlayCtx = activeCtx.childWithSharedScope(LayoutConstraints{
        .maxWidth = outerSize.width,
        .maxHeight = outerSize.height,
        .minWidth = 0.f,
        .minHeight = 0.f,
    }, activeCtx.hints());
    if (auto overlayNode = modifiers.overlay->mount(overlayCtx)) {
      wrapper->appendChild(std::move(overlayNode));
    }
  }

  LayoutHints const relayoutHints = activeCtx.hints();
  rawWrapper->setLayoutConstraints(activeCtx.constraints());
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
