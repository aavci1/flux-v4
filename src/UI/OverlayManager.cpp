#include <Flux/UI/Overlay.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/SceneGraphBounds.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/LayoutTree.hpp>
#include <Flux/UI/RenderContext.hpp>
#include <Flux/UI/RenderLayoutTree.hpp>
#include <Flux/UI/Views/Popover.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>

namespace flux {

namespace {

Rect applyAnchorOutsets(Rect rect, EdgeInsets const &outsets) {
    float const left = std::max(0.f, outsets.left);
    float const right = std::max(0.f, outsets.right);
    float const top = std::max(0.f, outsets.top);
    float const bottom = std::max(0.f, outsets.bottom);
    return Rect {
        rect.x - left,
        rect.y - top,
        rect.width + left + right,
        rect.height + top + bottom,
    };
}

char const *overlayPlacementName(OverlayConfig::Placement placement) {
    switch (placement) {
    case OverlayConfig::Placement::Below:
        return "below";
    case OverlayConfig::Placement::Above:
        return "above";
    case OverlayConfig::Placement::End:
        return "end";
    case OverlayConfig::Placement::Start:
        return "start";
    }
    return "unknown";
}

float resolveCrossAlignedX(Size win, Rect const &anchor, Rect contentBounds,
                           OverlayConfig::CrossAlignment alignment) {
    float const centeredX = (anchor.x + anchor.width * 0.5f) - (contentBounds.x + contentBounds.width * 0.5f);
    float const startAlignedX = anchor.x - contentBounds.x;
    float const endAlignedX = (anchor.x + anchor.width) - (contentBounds.x + contentBounds.width);

    auto fitsHorizontally = [win, width = contentBounds.width](float x) {
        return x >= 0.f && (x + width) <= win.width;
    };

    switch (alignment) {
    case OverlayConfig::CrossAlignment::Center:
        return centeredX;
    case OverlayConfig::CrossAlignment::Start:
        return startAlignedX;
    case OverlayConfig::CrossAlignment::End:
        return endAlignedX;
    case OverlayConfig::CrossAlignment::PreferStart:
        if (fitsHorizontally(startAlignedX)) {
            return startAlignedX;
        }
        if (fitsHorizontally(endAlignedX)) {
            return endAlignedX;
        }
        return startAlignedX;
    case OverlayConfig::CrossAlignment::PreferEnd:
        if (fitsHorizontally(endAlignedX)) {
            return endAlignedX;
        }
        if (fitsHorizontally(startAlignedX)) {
            return startAlignedX;
        }
        return endAlignedX;
    }
    return centeredX;
}

} // namespace

bool OverlayId::isValid() const noexcept {
    return value != 0;
}

bool OverlayManager::hasOverlays() const noexcept {
    return !overlays_.empty();
}

std::vector<std::unique_ptr<OverlayEntry>> const &OverlayManager::entries() const {
    return overlays_;
}

OverlayEntry const *OverlayManager::top() const {
    if (overlays_.empty()) {
        return nullptr;
    }
    return overlays_.back().get();
}

LayoutConstraints OverlayManager::resolveConstraints(Size windowSize, OverlayConfig const &config) const {
    LayoutConstraints cs {};
    cs.minWidth = 0.f;
    cs.minHeight = 0.f;
    float maxW = windowSize.width;
    float maxH = windowSize.height;
    if (config.maxSize.has_value()) {
        Size const &ms = *config.maxSize;
        // Width 0: treat as unset (measured width not ready); do not clamp to 0 or the card disappears.
        if (std::isfinite(ms.width) && ms.width > 0.f) {
            maxW = std::min(maxW, ms.width);
        }
        if (std::isfinite(ms.height) && ms.height > 0.f) {
            maxH = std::min(maxH, ms.height);
        }
        // height <= 0: leave maxH as window height; PopoverCalloutShape also clamps using Popover::maxSize in build.
    }
    cs.maxWidth = std::max(0.f, maxW);
    cs.maxHeight = std::max(0.f, maxH);
    return cs;
}

Rect OverlayManager::resolveFrame(Size win, OverlayConfig const &cfg, Rect contentBounds) const {
    if (!cfg.anchor.has_value()) {
        return {0.f, 0.f, win.width, win.height};
    }
    Rect const &a = *cfg.anchor;
    float const cy = a.y + a.height * 0.5f;
    // Geometric center of drawn content in root-local coords (may be != (W/2, H/2) if min x/y != 0).
    float const centerLocalY = contentBounds.y + contentBounds.height * 0.5f;
    // Tip Y for Below: top of drawable union (arrow tip points up at anchor bottom).
    float const tipTopLocalY = contentBounds.y;
    // Tip Y for Above: bottom of drawable union (arrow tip points down at anchor top).
    float const tipBottomLocalY = contentBounds.y + contentBounds.height;
    float x = 0.f;
    float y = 0.f;
    // Center the arrow horizontally on the anchor; vertically align the tip to the anchor edge.
    switch (cfg.placement) {
    case OverlayConfig::Placement::Below:
        x = resolveCrossAlignedX(win, a, contentBounds, cfg.crossAlignment);
        y = a.y + a.height - tipTopLocalY;
        break;
    case OverlayConfig::Placement::Above:
        x = resolveCrossAlignedX(win, a, contentBounds, cfg.crossAlignment);
        y = a.y - tipBottomLocalY;
        break;
    case OverlayConfig::Placement::End:
        x = a.x + a.width;
        y = cy - centerLocalY;
        break;
    case OverlayConfig::Placement::Start:
        x = a.x - contentBounds.width;
        y = cy - centerLocalY;
        break;
    }
    x += cfg.offset.x;
    y += cfg.offset.y;
    x = std::clamp(x, 0.f, std::max(0.f, win.width - contentBounds.width));
    y = std::clamp(y, 0.f, std::max(0.f, win.height - contentBounds.height));
    return {x, y, contentBounds.width, contentBounds.height};
}

void OverlayManager::insertOverlayBackdropChrome(OverlayEntry &entry, Size windowSize, Runtime &runtime,
                                                 bool dismissOnBackdropTap) {
    SceneGraph &g = entry.graph;
    if (!g.node<LayerNode>(g.root())) {
        return;
    }

    float const w = windowSize.width;
    float const h = windowSize.height;
    // Window::render translates the overlay graph by resolvedFrame (popover position). Offset the
    // full-window backdrop/capture so they still cover (0,0)—(W,H) in screen space after that transform.
    float const ox = -entry.resolvedFrame.x;
    float const oy = -entry.resolvedFrame.y;

    RectNode backdrop {};
    backdrop.bounds = Rect {ox, oy, w, h};
    backdrop.fill = FillStyle::solid(entry.config.backdropColor);
    backdrop.stroke = StrokeStyle::none();
    NodeId const backdropId = g.addRect(g.root(), std::move(backdrop));

    RectNode capture {};
    capture.bounds = Rect {ox, oy, w, h};
    capture.fill = FillStyle::none();
    capture.stroke = StrokeStyle::none();
    NodeId const captureId = g.addRect(g.root(), std::move(capture));

    EventHandlers captureHandlers {};
    captureHandlers.stableTargetKey = ComponentKey {};
    if (dismissOnBackdropTap) {
        OverlayId const oid = entry.id;
        Window &wnd = runtime.window();
        captureHandlers.onPointerDown = [oid, &wnd](Point) { wnd.removeOverlay(oid); };
    } else {
        captureHandlers.onPointerDown = [](Point) {};
    }
    entry.eventMap.insert(captureId, std::move(captureHandlers));

    // Re-fetch root after addRect: NodeStore::insert may reallocate slots_, invalidating
    // any LayerNode* taken before new nodes were inserted.
    auto *rootLayer = g.node<LayerNode>(g.root());
    if (!rootLayer) {
        return;
    }
    std::vector<NodeId> newOrder;
    newOrder.reserve(rootLayer->children.size());
    newOrder.push_back(backdropId);
    newOrder.push_back(captureId);
    for (NodeId c : rootLayer->children) {
        if (c != backdropId && c != captureId) {
            newOrder.push_back(c);
        }
    }
    g.reorder(g.root(), newOrder);
}

void OverlayManager::rebuild(Size windowSize, Runtime &runtime) {
    for (std::unique_ptr<OverlayEntry> &up : overlays_) {
        StateStore *const prevCurrent = StateStore::current();
        OverlayEntry &entry = *up;
        if (entry.config.anchorTrackLeafKey.has_value() && !entry.config.anchorTrackLeafKey->empty()) {
            if (auto r = runtime.layoutRectForLeafKeyPrefix(*entry.config.anchorTrackLeafKey)) {
                entry.config.anchor = *r;
            }
        } else if (entry.config.anchorTrackComponentKey.has_value() &&
                   !entry.config.anchorTrackComponentKey->empty()) {
            if (auto r = runtime.layoutRectForKey(*entry.config.anchorTrackComponentKey)) {
                entry.config.anchor = *r;
            }
        }
        if (entry.config.anchor.has_value() && entry.config.anchorMaxHeight.has_value()) {
            entry.config.anchor->height =
                std::min(entry.config.anchor->height, *entry.config.anchorMaxHeight);
        }
        if (entry.config.anchor.has_value() && !entry.config.anchorOutsets.isZero()) {
            entry.config.anchor = applyAnchorOutsets(*entry.config.anchor, entry.config.anchorOutsets);
        }
        if (entry.config.popoverPreferredPlacement.has_value() && entry.content.has_value() &&
            entry.config.anchor.has_value()) {
            PopoverPlacement const resolved = resolvePopoverPlacement(
                *entry.config.popoverPreferredPlacement, entry.config.anchor, entry.config.maxSize,
                entry.config.popoverGapTotal, windowSize
            );
            entry.config.placement = overlayPlacementFromPopover(resolved);
            entry.config.offset = popoverOverlayGapOffset(resolved, entry.config.popoverGap);
            if (entry.onPlacementResolved) {
                entry.onPlacementResolved(resolved);
            }
        }
        entry.graph.clear();
        entry.layoutTree.beginBuild();
        entry.eventMap.clear();

        entry.stateStore->beginRebuild();
        entry.stateStore->setOverlayScope(entry.id.value);
        StateStore::setCurrent(entry.stateStore.get());

        layoutEngine_.resetForBuild();
        overlayMeasureCache_.clear();
        LayoutConstraints const cs = resolveConstraints(windowSize, entry.config);
        LayoutContext lctx {Application::instance().textSystem(), layoutEngine_, entry.layoutTree, &overlayMeasureCache_};
        lctx.pushConstraints(cs);
        EnvironmentLayer windowEnvBaseline = runtime.window().environmentLayer();
        EnvironmentStack::current().push(std::move(windowEnvBaseline));
        layoutEngine_.setChildFrame(Rect {0.f, 0.f, cs.maxWidth, cs.maxHeight});
        if (entry.content.has_value()) {
            entry.content->layout(lctx);
        }
        EnvironmentStack::current().pop();
        lctx.popConstraints();
        entry.layoutTree.endBuild();

        RenderContext rctx {entry.graph, entry.eventMap, Application::instance().textSystem()};
        rctx.pushConstraints(cs);
        renderLayoutTree(entry.layoutTree, rctx);
        rctx.popConstraints();

        StateStore::setCurrent(prevCurrent);
        entry.stateStore->setOverlayScope(std::nullopt);
        entry.stateStore->endRebuild();

        Rect contentBounds = measureRootContentBounds(entry.graph);
        if (contentBounds.width <= 0.f || !std::isfinite(contentBounds.width)) {
            contentBounds.width = 1.f;
        }
        if (contentBounds.height <= 0.f || !std::isfinite(contentBounds.height)) {
            contentBounds.height = 1.f;
        }

        entry.resolvedFrame = resolveFrame(windowSize, entry.config, contentBounds);

        if (!entry.config.debugName.empty()) {
            if (entry.config.anchor.has_value()) {
                Rect const &anchor = *entry.config.anchor;
                std::fprintf(stderr,
                             "[overlay:%s] rebuild anchor=(%.1f, %.1f, %.1f, %.1f) content=(%.1f, %.1f, %.1f, %.1f) frame=(%.1f, %.1f, %.1f, %.1f) placement=%s offset=(%.1f, %.1f)\n",
                             entry.config.debugName.c_str(), anchor.x, anchor.y, anchor.width,
                             anchor.height, contentBounds.x, contentBounds.y, contentBounds.width,
                             contentBounds.height, entry.resolvedFrame.x, entry.resolvedFrame.y,
                             entry.resolvedFrame.width, entry.resolvedFrame.height,
                             overlayPlacementName(entry.config.placement), entry.config.offset.x,
                             entry.config.offset.y);
            } else {
                std::fprintf(stderr,
                             "[overlay:%s] rebuild anchor=(none) content=(%.1f, %.1f, %.1f, %.1f) frame=(%.1f, %.1f, %.1f, %.1f) placement=%s offset=(%.1f, %.1f)\n",
                             entry.config.debugName.c_str(), contentBounds.x, contentBounds.y,
                             contentBounds.width, contentBounds.height, entry.resolvedFrame.x,
                             entry.resolvedFrame.y, entry.resolvedFrame.width,
                             entry.resolvedFrame.height, overlayPlacementName(entry.config.placement),
                             entry.config.offset.x, entry.config.offset.y);
            }
        }

        if (entry.config.modal) {
            insertOverlayBackdropChrome(entry, windowSize, runtime, false);
        } else if (entry.config.backdropColor.a > 0.001f) {
            insertOverlayBackdropChrome(entry, windowSize, runtime, entry.config.dismissOnOutsideTap);
        }

        runtime.syncModalOverlayFocusAfterRebuild(entry);
    }
}

OverlayId OverlayManager::push(Element content, OverlayConfig config, Runtime *runtime) {
    auto entry = std::make_unique<OverlayEntry>();
    entry->id = OverlayId {nextId_++};
    entry->content.emplace(std::move(content));
    entry->config = std::move(config);
    if (entry->content.has_value()) {
        if (Popover *pop = detail::popoverOverlayStateIf(*entry->content)) {
            entry->onPlacementResolved = [pop](PopoverPlacement p) { pop->resolvedPlacement = p; };
        }
    }
    overlays_.push_back(std::move(entry));

    if (runtime) {
        runtime->onOverlayPushed(*overlays_.back());
        Size const sz = runtime->window().getSize();
        rebuild(sz, *runtime);
        runtime->window().requestRedraw();
    } else {
        Application::instance().markReactiveDirty();
    }
    return overlays_.back()->id;
}

void OverlayManager::remove(OverlayId id, Runtime *runtime) {
    if (!id.isValid()) {
        return;
    }
    for (auto it = overlays_.begin(); it != overlays_.end(); ++it) {
        if ((*it)->id.value == id.value) {
            std::unique_ptr<OverlayEntry> removed = std::move(*it);
            overlays_.erase(it);
            if (runtime) {
                runtime->onOverlayRemoved(*removed);
            }
            if (!runtime || !runtime->shuttingDown()) {
                Application::instance().markReactiveDirty();
            }
            if (removed->config.onDismiss) {
                removed->config.onDismiss();
            }
            return;
        }
    }
}

void OverlayManager::clear(Runtime *runtime, bool invokeDismissCallbacks) {
    while (!overlays_.empty()) {
        OverlayEntry &top = *overlays_.back();
        if (invokeDismissCallbacks && top.config.onDismiss) {
            top.config.onDismiss();
        }
        std::unique_ptr<OverlayEntry> removed = std::move(overlays_.back());
        overlays_.pop_back();
        if (runtime) {
            runtime->onOverlayRemoved(*removed);
        }
    }
    if (!runtime || !runtime->shuttingDown()) {
        Application::instance().markReactiveDirty();
    }
}

} // namespace flux
