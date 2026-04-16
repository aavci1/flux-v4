#pragma once

/// \file ContainerScope.hpp
///
/// RAII helpers for layout containers — measure vs layout passes.

#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/StateStore.hpp>

#include <Flux/UI/Detail/LayoutDebugDump.hpp>

#include <cassert>
#include <cstddef>
#include <vector>

namespace flux {

/// Manages the layout-side container protocol (mirrors the former \c ContainerBuildScope).
class ContainerLayoutScope {
public:
  explicit ContainerLayoutScope(LayoutContext& ctx)
      : ctx(ctx)
      , le(ctx.layoutEngine()) {
#ifndef NDEBUG
    layerWorldDepth0_ = ctx.debugLayerWorldStackDepth();
    constraintDepth0_ = ctx.debugConstraintStackDepth();
    keyPathDepth0_ = ctx.debugKeyPathDepth();
    savedDepth0_ = ctx.debugSavedChildDepth();
#endif
    stableKey_ = ctx.leafComponentKey();
    if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
      ctx.advanceChildSlot();
    }
    parentFrame = le.consumeAssignedFrame();
    outer = ctx.constraints();
    ctx.pushChildIndex();
  }

  void pushStandardLayer(bool clip, float assignedW, float assignedH) {
    Mat3 local = Mat3::translate(parentFrame.x, parentFrame.y);
    if (clip && assignedW > 0.f && assignedH > 0.f) {
      // clip rect is in layer-local space — recorded on spec for render phase
    }

    LayoutNode n{};
    n.kind = LayoutNode::Kind::Container;
    n.frame = parentFrame;
    n.constraints = outer;
    n.hints = ctx.hints();
    n.componentKey = stableKey_;
    n.containerSpec.kind = ContainerLayerSpec::Kind::Standard;
    n.containerSpec.clip = clip;
    n.containerSpec.clipW = assignedW;
    n.containerSpec.clipH = assignedH;
    n.element = ctx.currentElement();
    // pushLayoutNode uses currentLayerWorldTransform() (parent's), then we push child transform.
    LayoutNodeId const id = ctx.pushLayoutNode(std::move(n));
    ctx.pushLayerWorldTransform(local);
    ctx.registerCompositeSubtreeRootIfPending(id);
    ctx.pushLayoutParent(id);
    layerPushed_ = true;
  }

  void pushOffsetScrollLayer(Point scrollOffset) {
    float const ox = parentFrame.x - scrollOffset.x;
    float const oy = parentFrame.y - scrollOffset.y;
    Mat3 const local = Mat3::translate(ox, oy);

    LayoutNode n{};
    n.kind = LayoutNode::Kind::Container;
    n.frame = parentFrame;
    n.constraints = outer;
    n.hints = ctx.hints();
    n.componentKey = stableKey_;
    n.containerSpec.kind = ContainerLayerSpec::Kind::OffsetScroll;
    n.containerSpec.scrollOffset = scrollOffset;
    n.element = ctx.currentElement();
    // pushLayoutNode uses currentLayerWorldTransform() (parent's), then we push child transform.
    LayoutNodeId const id = ctx.pushLayoutNode(std::move(n));
    ctx.pushLayerWorldTransform(local);
    ctx.registerCompositeSubtreeRootIfPending(id);
    ctx.pushLayoutParent(id);
    layerPushed_ = true;
  }

  void pushScaleAroundCenterLayer(Mat3 const& fullLayerTransform) {
    LayoutNode n{};
    n.kind = LayoutNode::Kind::Container;
    n.frame = parentFrame;
    n.constraints = outer;
    n.hints = ctx.hints();
    n.componentKey = stableKey_;
    n.containerSpec.kind = ContainerLayerSpec::Kind::ScaleAroundCenter;
    n.containerSpec.customTransform = fullLayerTransform;
    n.element = ctx.currentElement();
    // pushLayoutNode uses currentLayerWorldTransform() (parent's), then we push child transform.
    LayoutNodeId const id = ctx.pushLayoutNode(std::move(n));
    ctx.pushLayerWorldTransform(fullLayerTransform);
    ctx.registerCompositeSubtreeRootIfPending(id);
    ctx.pushLayoutParent(id);
    layerPushed_ = true;
  }

  std::vector<Size> measureChildren(std::vector<Element> const& children, LayoutConstraints const& childCs,
                                   LayoutHints childHints = {}) {
    std::vector<Size> sizes;
    sizes.reserve(children.size());
    for (Element const& ch : children) {
      Size size{};
      if (!ch.tryCachedMeasure(ctx, childCs, childHints, ctx.textSystem(), size)) {
        size = ch.measure(ctx, childCs, childHints, ctx.textSystem());
      }
      sizes.push_back(size);
    }
    if (StateStore* store = StateStore::current()) {
      store->resetSlotCursors();
    }
    ctx.rewindChildKeyIndex();
    return sizes;
  }

  Size measureChild(Element const& child, LayoutConstraints const& childCs, LayoutHints childHints = {}) {
    Size sz{};
    if (!child.tryCachedMeasure(ctx, childCs, childHints, ctx.textSystem(), sz)) {
      sz = child.measure(ctx, childCs, childHints, ctx.textSystem());
    }
    if (StateStore* store = StateStore::current()) {
      store->resetSlotCursors();
    }
    ctx.rewindChildKeyIndex();
    return sz;
  }

  void layoutChild(Element const& child, Rect frame, LayoutConstraints const& cs, LayoutHints hints = {}) {
    le.setChildFrame(frame);
    ctx.pushConstraints(cs, std::move(hints));
    if (!child.tryRetainedLayout(ctx)) {
      child.layout(ctx);
    }
    ctx.popConstraints();
  }

  void logContainer(char const* tag) const { layoutDebugLogContainer(tag, outer, parentFrame); }

  ComponentKey const& stableKey() const noexcept { return stableKey_; }

  ~ContainerLayoutScope() {
    ctx.popChildIndex();
    if (layerPushed_) {
      ctx.popLayoutParent();
      ctx.popLayerWorldTransform();
    }
#ifndef NDEBUG
    assert(ctx.debugLayerWorldStackDepth() == layerWorldDepth0_);
    assert(ctx.debugConstraintStackDepth() == constraintDepth0_);
    assert(ctx.debugKeyPathDepth() == keyPathDepth0_);
    assert(ctx.debugSavedChildDepth() == savedDepth0_);
#endif
  }

  ContainerLayoutScope(ContainerLayoutScope const&) = delete;
  ContainerLayoutScope& operator=(ContainerLayoutScope const&) = delete;

  LayoutContext& ctx;
  LayoutEngine& le;
  Rect parentFrame{};
  LayoutConstraints outer{};

private:
  bool layerPushed_{false};
  ComponentKey stableKey_{};
#ifndef NDEBUG
  std::size_t layerWorldDepth0_{};
  std::size_t constraintDepth0_{};
  std::size_t keyPathDepth0_{};
  std::size_t savedDepth0_{};
#endif
};

class ContainerMeasureScope {
public:
  explicit ContainerMeasureScope(LayoutContext& ctx) : ctx_(ctx) {
#ifndef NDEBUG
    keyPathDepth0_ = ctx_.debugKeyPathDepth();
    savedDepth0_ = ctx_.debugSavedChildDepth();
#endif
    if (!ctx_.consumeCompositeBodySubtreeRootSkip()) {
      ctx_.advanceChildSlot();
    }
    ctx_.pushChildIndex();
  }

  ~ContainerMeasureScope() {
    ctx_.popChildIndex();
#ifndef NDEBUG
    assert(ctx_.debugKeyPathDepth() == keyPathDepth0_);
    assert(ctx_.debugSavedChildDepth() == savedDepth0_);
#endif
  }

  ContainerMeasureScope(ContainerMeasureScope const&) = delete;
  ContainerMeasureScope& operator=(ContainerMeasureScope const&) = delete;

private:
  LayoutContext& ctx_;
#ifndef NDEBUG
  std::size_t keyPathDepth0_{};
  std::size_t savedDepth0_{};
#endif
};

} // namespace flux
