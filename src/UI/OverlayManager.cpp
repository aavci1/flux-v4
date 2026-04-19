#include <Flux/UI/Overlay.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/InteractionData.hpp>
#include <Flux/Scene/RectSceneNode.hpp>
#include <Flux/Scene/SceneTree.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/Views/Popover.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <typeinfo>
#include <utility>

#include "Scene/SceneGeometry.hpp"
#include "UI/BuildSession.hpp"
#include "UI/Layout/Algorithms/OverlayLayout.hpp"

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

bool isPlainGroup(SceneNode const& node) {
    return node.kind() == SceneNodeKind::Group && typeid(node) == typeid(SceneNode);
}

NodeId overlayRootId() {
    return NodeId{1ull};
}

NodeId overlayContentId() {
    return SceneTree::childId(overlayRootId(), LocalId::fromString("$content"));
}

NodeId overlayBackdropId() {
    return SceneTree::childId(overlayRootId(), LocalId::fromString("$backdrop"));
}

NodeId overlayCaptureId() {
    return SceneTree::childId(overlayRootId(), LocalId::fromString("$capture"));
}

Size measureOverlayContent(Element const& element, LayoutConstraints const& constraints,
                          TextSystem& textSystem) {
    MeasureContext measureContext{textSystem};
    measureContext.pushConstraints(constraints, LayoutHints{});
    ComponentKey const rootKey{LocalId::fromIndex(0)};
    measureContext.resetTraversalState(rootKey);
    if (element.isComposite() && element.typeTag() == ElementType::Unknown) {
        measureContext.setMeasurementRootKey(rootKey);
    } else {
        measureContext.clearMeasurementRootKey();
    }
    measureContext.setCurrentElement(&element);
    return element.measure(measureContext, constraints, LayoutHints{}, textSystem);
}

std::unique_ptr<SceneNode> extractOverlayContentRoot(std::unique_ptr<SceneNode> root) {
    if (!root) {
        return nullptr;
    }
    if (root->id() == overlayContentId()) {
        return root;
    }
    if (!isPlainGroup(*root)) {
        return nullptr;
    }
    std::vector<std::unique_ptr<SceneNode>> children = root->releaseChildren();
    for (std::unique_ptr<SceneNode>& child : children) {
        if (child && child->id() == overlayContentId()) {
            return std::move(child);
        }
    }
    return nullptr;
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

OverlayEntry* OverlayManager::find(OverlayId id) {
    if (!id.isValid()) {
        return nullptr;
    }
    for (std::unique_ptr<OverlayEntry>& up : overlays_) {
        if (up->id == id) {
            return up.get();
        }
    }
    return nullptr;
}

OverlayEntry const* OverlayManager::find(OverlayId id) const {
    return const_cast<OverlayManager*>(this)->find(id);
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
    return layout::resolveOverlayFrame(win, cfg, contentBounds);
}

void OverlayManager::insertOverlayBackdropChrome(SceneNode &root, OverlayEntry &entry, Size windowSize,
                                                 Runtime &runtime, bool dismissOnBackdropTap) {
    float const w = std::max(0.f, windowSize.width);
    float const h = std::max(0.f, windowSize.height);
    float const ox = -entry.resolvedFrame.x;
    float const oy = -entry.resolvedFrame.y;

    auto backdrop = std::make_unique<RectSceneNode>(overlayBackdropId());
    backdrop->position = Point{ox, oy};
    backdrop->size = Size{w, h};
    backdrop->fill = FillStyle::solid(entry.config.backdropColor);
    backdrop->stroke = StrokeStyle::none();
    backdrop->shadow = ShadowStyle::none();
    backdrop->cornerRadius = {};
    backdrop->recomputeBounds();
    root.appendChild(std::move(backdrop));

    auto capture = std::make_unique<RectSceneNode>(overlayCaptureId());
    capture->position = Point{ox, oy};
    capture->size = Size{w, h};
    capture->fill = FillStyle::none();
    capture->stroke = StrokeStyle::none();
    capture->shadow = ShadowStyle::none();
    capture->cornerRadius = {};
    auto interaction = std::make_unique<InteractionData>();
    if (dismissOnBackdropTap) {
        OverlayId const oid = entry.id;
        Window &wnd = runtime.window();
        interaction->onPointerDown = [oid, &wnd](Point) { wnd.removeOverlay(oid); };
    } else {
        interaction->onPointerDown = [](Point) {};
    }
    capture->setInteraction(std::move(interaction));
    capture->recomputeBounds();
    root.appendChild(std::move(capture));
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
        entry.stateStore->beginRebuild();
        entry.stateStore->setOverlayScope(entry.id.value);
        StateStore::setCurrent(entry.stateStore.get());
        LayoutConstraints const cs = resolveConstraints(windowSize, entry.config);

        if (entry.config.popoverPreferredPlacement.has_value() && entry.content.has_value() &&
            entry.config.anchor.has_value()) {
            if (Popover* pop = detail::popoverOverlayStateIf(*entry.content)) {
                ComponentKey const overlayRootKey{LocalId::fromIndex(0)};
                PopoverPlacement const measuredPlacement = *entry.config.popoverPreferredPlacement;
                pop->resolvedPlacement = measuredPlacement;
                Size const measured = measureOverlayContent(*entry.content, cs, Application::instance().textSystem());
                PopoverPlacement const resolved = resolveMeasuredPopoverPlacement(
                    *entry.config.popoverPreferredPlacement, entry.config.anchor, measured,
                    entry.config.popoverGap, windowSize
                );
                entry.config.placement = overlayPlacementFromPopover(resolved);
                entry.config.offset = popoverOverlayGapOffset(resolved, entry.config.popoverGap);
                if (entry.onPlacementResolved) {
                    entry.onPlacementResolved(resolved);
                }
                if (resolved != measuredPlacement) {
                    entry.stateStore->discardCurrentRebuildBody(overlayRootKey);
                }
            } else {
                PopoverPlacement const resolved = resolvePopoverPlacement(
                    *entry.config.popoverPreferredPlacement, entry.config.anchor, entry.config.maxSize,
                    entry.config.popoverGapTotal, windowSize
                );
                entry.config.placement = overlayPlacementFromPopover(resolved);
                entry.config.offset = popoverOverlayGapOffset(resolved, entry.config.popoverGap);
            }
        }

        std::unique_ptr<SceneNode> existingContent = extractOverlayContentRoot(entry.sceneTree.takeRoot());
        BuildSession buildSession{
            Application::instance().textSystem(),
            EnvironmentStack::current(),
            runtime.window().environmentLayer(),
            &entry.sceneGeometry,
        };
        std::unique_ptr<SceneNode> contentRoot =
            buildSession.buildRoot(resolveOverlayRootScene(entry.content), overlayContentId(), cs, std::move(existingContent));

        Rect contentBounds = contentRoot ? scene::offsetRect(contentRoot->bounds, contentRoot->position) : Rect{};
        if (contentBounds.width <= 0.f || !std::isfinite(contentBounds.width)) {
            contentBounds.width = 1.f;
        }
        if (contentBounds.height <= 0.f || !std::isfinite(contentBounds.height)) {
            contentBounds.height = 1.f;
        }

        entry.resolvedFrame = resolveFrame(windowSize, entry.config, contentBounds);

        auto root = std::make_unique<SceneNode>(overlayRootId());

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
            insertOverlayBackdropChrome(*root, entry, windowSize, runtime, false);
        } else if (entry.config.backdropColor.a > 0.001f) {
            insertOverlayBackdropChrome(*root, entry, windowSize, runtime, entry.config.dismissOnOutsideTap);
        }

        if (contentRoot) {
            root->appendChild(std::move(contentRoot));
        }
        root->recomputeBounds();
        entry.sceneTree.setRoot(std::move(root));

        StateStore::setCurrent(prevCurrent);
        entry.stateStore->setOverlayScope(std::nullopt);
        entry.stateStore->endRebuild();

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
