#pragma once

/// \file ContainerScope.hpp
///
/// RAII helpers that extract the duplicated 7-step layout container protocol into
/// two scope objects — one for `build`, one for `measure`.  Internal header; not
/// part of the public API.

#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/StateStore.hpp>

#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include <Flux/UI/Detail/LayoutDebugDump.hpp>

#include <cassert>
#include <cstddef>
#include <vector>

namespace flux {

/// Manages the full build-side container protocol: slot consumption, parent state
/// read, layer push/pop, child index push/pop, measure-rewind, and per-child
/// frame+constraints wrapping.
///
/// Usage (standard layer):
///   ContainerBuildScope scope(ctx);
///   scope.pushStandardLayer(clip, assignedW, assignedH);
///   auto sizes = scope.measureChildren(children, childCs);
///   for (...) scope.buildChild(child, frame, cs);
///
/// Usage (custom layer, e.g. with offset/scale transform):
///   ContainerBuildScope scope(ctx);
///   Size sz = scope.measureChild(child, childCs);
///   NodeId lid = ctx.graph().addLayer(ctx.parentLayer(), myLayer);
///   scope.pushCustomLayer(lid);
///   scope.buildChild(child, frame, cs);
class ContainerBuildScope {
public:
  explicit ContainerBuildScope(BuildContext& ctx)
      : ctx(ctx), le(ctx.layoutEngine()) {
#ifndef NDEBUG
    layerDepth0_ = ctx.debugLayerStackDepth();
    constraintDepth0_ = ctx.debugConstraintStackDepth();
    keyPathDepth0_ = ctx.debugKeyPathDepth();
    savedDepth0_ = ctx.debugSavedChildDepth();
#endif
    if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
      ctx.advanceChildSlot();
    }
    parentFrame = le.consumeAssignedFrame();
    outer = ctx.constraints();
    ctx.pushChildIndex();
  }

  void pushStandardLayer(bool clip, float assignedW, float assignedH) {
    LayerNode layer{};
    if (parentFrame.width > 0.f || parentFrame.height > 0.f) {
      layer.transform = Mat3::translate(parentFrame.x, parentFrame.y);
    }
    if (clip && assignedW > 0.f && assignedH > 0.f) {
      layer.clip = Rect{0.f, 0.f, assignedW, assignedH};
    }
    NodeId const layerId =
        ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
    ctx.registerCompositeSubtreeRootIfPending(layerId);
    ctx.pushLayer(layerId);
    layerPushed_ = true;
  }

  void pushCustomLayer(NodeId layerId) {
    ctx.registerCompositeSubtreeRootIfPending(layerId);
    ctx.pushLayer(layerId);
    layerPushed_ = true;
  }

  std::vector<Size> measureChildren(std::vector<Element> const& children,
                                    LayoutConstraints const& childCs,
                                    LayoutHints childHints = {}) {
    std::vector<Size> sizes;
    sizes.reserve(children.size());
    for (Element const& ch : children) {
      sizes.push_back(ch.measure(ctx, childCs, childHints, ctx.textSystem()));
    }
    if (StateStore* store = StateStore::current()) {
      store->resetSlotCursors();
    }
    ctx.rewindChildKeyIndex();
    return sizes;
  }

  Size measureChild(Element const& child, LayoutConstraints const& childCs,
                    LayoutHints childHints = {}) {
    Size const sz = child.measure(ctx, childCs, childHints, ctx.textSystem());
    if (StateStore* store = StateStore::current()) {
      store->resetSlotCursors();
    }
    ctx.rewindChildKeyIndex();
    return sz;
  }

  void buildChild(Element const& child, Rect frame, LayoutConstraints const& cs,
                  LayoutHints hints = {}) {
    le.setChildFrame(frame);
    ctx.pushConstraints(cs, std::move(hints));
    child.build(ctx);
    ctx.popConstraints();
  }

  /// Log this container after \ref measureChildren / \ref measureChild so measured sizes are in the map.
  void logContainer(char const* tag) const {
    layoutDebugLogContainer(tag, outer, parentFrame);
  }

  ~ContainerBuildScope() {
    ctx.popChildIndex();
    if (layerPushed_) {
      ctx.popLayer();
    }
#ifndef NDEBUG
    assert(ctx.debugLayerStackDepth() == layerDepth0_);
    assert(ctx.debugConstraintStackDepth() == constraintDepth0_);
    assert(ctx.debugKeyPathDepth() == keyPathDepth0_);
    assert(ctx.debugSavedChildDepth() == savedDepth0_);
#endif
  }

  ContainerBuildScope(ContainerBuildScope const&) = delete;
  ContainerBuildScope& operator=(ContainerBuildScope const&) = delete;

  BuildContext& ctx;
  LayoutEngine& le;
  Rect parentFrame{};
  LayoutConstraints outer{};

private:
  bool layerPushed_{false};
#ifndef NDEBUG
  std::size_t layerDepth0_{};
  std::size_t constraintDepth0_{};
  std::size_t keyPathDepth0_{};
  std::size_t savedDepth0_{};
#endif
};

/// Manages the measure-side container preamble: slot consumption and balanced
/// child index push/pop.
class ContainerMeasureScope {
public:
  explicit ContainerMeasureScope(BuildContext& ctx) : ctx_(ctx) {
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
  BuildContext& ctx_;
#ifndef NDEBUG
  std::size_t keyPathDepth0_{};
  std::size_t savedDepth0_{};
#endif
};

} // namespace flux
