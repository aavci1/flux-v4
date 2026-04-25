#include <Flux/UI/Element.hpp>

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/Detail/LayoutDebugDump.hpp>
#include <Flux/UI/Environment.hpp>

#include "UI/Layout/ContainerScope.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace flux {

namespace {

struct ChildLocalIdScope {
  MeasureContext& ctx;

  ChildLocalIdScope(MeasureContext& context, std::optional<std::string> const& explicitKey)
      : ctx(context) {
    if (explicitKey.has_value()) {
      ctx.pushExplicitChildLocalId(LocalId::fromString(*explicitKey));
    } else {
      ctx.pushExplicitChildLocalId(std::nullopt);
    }
  }

  ~ChildLocalIdScope() { ctx.popExplicitChildLocalId(); }
};

} // namespace

Size Element::measureWithModifiersImpl(MeasureContext& ctx, LayoutConstraints const& constraints,
                                       LayoutHints const& hints, TextSystem& textSystem) const {
  detail::ElementModifiers const& m = *modifiers_;
  float const padL = std::max(0.f, m.padding.left);
  float const padR = std::max(0.f, m.padding.right);
  float const padT = std::max(0.f, m.padding.top);
  float const padB = std::max(0.f, m.padding.bottom);
  float const padW = padL + padR;
  float const padH = padT + padB;
  LayoutConstraints innerCs = constraints;
  if (padW > 0.f || padH > 0.f) {
    if (std::isfinite(innerCs.maxWidth)) {
      innerCs.maxWidth -= padW;
    }
    if (std::isfinite(innerCs.maxHeight)) {
      innerCs.maxHeight -= padH;
    }
    innerCs.minWidth = std::max(0.f, innerCs.minWidth - padW);
    innerCs.minHeight = std::max(0.f, innerCs.minHeight - padH);
  }
  if (std::isfinite(innerCs.maxWidth)) {
    innerCs.minWidth = std::min(innerCs.minWidth, innerCs.maxWidth);
  }
  if (std::isfinite(innerCs.maxHeight)) {
    innerCs.minHeight = std::min(innerCs.minHeight, innerCs.maxHeight);
  }

  Size sz{};
  if (m.overlay) {
    ContainerMeasureScope scope(ctx);
    if (StateStore* const store = StateStore::current()) {
      store->pushCompositeElementModifiers(&m);
    }
    Size const szUnder = impl_->measure(ctx, innerCs, hints, textSystem);
    if (StateStore* const store = StateStore::current()) {
      store->popCompositeElementModifiers();
    }
    Size const szOver = m.overlay->measure(ctx, innerCs, hints, textSystem);
    sz.width = std::max(szUnder.width, szOver.width) + padW;
    sz.height = std::max(szUnder.height, szOver.height) + padH;
  } else {
    ContainerMeasureScope scope(ctx);
    if (StateStore* const store = StateStore::current()) {
      store->pushCompositeElementModifiers(&m);
    }
    sz = impl_->measure(ctx, innerCs, hints, textSystem);
    if (StateStore* const store = StateStore::current()) {
      store->popCompositeElementModifiers();
    }
    sz.width += padW;
    sz.height += padH;
  }
  if (m.sizeWidth > 0.f) {
    sz.width = m.sizeWidth;
  }
  if (m.sizeHeight > 0.f) {
    sz.height = m.sizeHeight;
  }
  return sz;
}

Size Element::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                      LayoutHints const& hints, TextSystem& textSystem) const {
  ChildLocalIdScope const childIdScope{ctx, key_};
  Element const* const prevEl = ctx.currentElement();
  ctx.setCurrentElement(this);
  if (modifiers_ && modifiers_->hasInteraction()) {
    if (StateStore* const store = StateStore::current()) {
      store->recordInteraction(ctx.currentElementKey(), *modifiers_);
    }
  }
  if (envLayer_) {
    EnvironmentStack::current().push(*envLayer_);
  }
  Size const sz = modifiers_ && modifiers_->needsModifierPass() ? measureWithModifiersImpl(ctx, constraints, hints, textSystem)
                                                                : impl_->measure(ctx, constraints, hints, textSystem);
  if (envLayer_) {
    EnvironmentStack::current().pop();
  }
  ctx.setCurrentElement(prevEl);
  layoutDebugRecordMeasure(measureId_, constraints, sz);
#ifndef NDEBUG
  assert(std::isfinite(sz.width) && std::isfinite(sz.height));
  assert(sz.width >= 0.f && sz.height >= 0.f);
#endif
  return sz;
}

} // namespace flux
