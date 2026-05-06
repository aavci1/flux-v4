#include <Flux/SceneGraph/SceneRenderer.hpp>

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/SceneGraph/PathNode.hpp>
#include <Flux/SceneGraph/RasterCacheNode.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/RenderNode.hpp>
#include <Flux/SceneGraph/Renderer.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>

#if defined(FLUX_PLATFORM_MACOS)
#include "Graphics/Metal/MetalCanvas.hpp"
#include "Graphics/Metal/MetalCanvasTypes.hpp"
#include "Graphics/Metal/MetalFrameRecorder.hpp"
#endif
#include "SceneGraph/SceneBounds.hpp"
#include "SceneGraph/SceneNodeInternal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>

#include "Debug/PerfCounters.hpp"

namespace flux::scenegraph {

namespace {

constexpr bool kEnablePreparedRenderCache = true;

bool isZeroOffset(Point offset) noexcept {
    return offset.x == 0.f && offset.y == 0.f;
}

bool isIdentityTransform(Mat3 const& matrix, float eps = 1e-6f) noexcept {
    return std::abs(matrix.m[0] - 1.f) <= eps &&
           std::abs(matrix.m[1]) <= eps &&
           std::abs(matrix.m[2]) <= eps &&
           std::abs(matrix.m[3]) <= eps &&
           std::abs(matrix.m[4] - 1.f) <= eps &&
           std::abs(matrix.m[5]) <= eps &&
           std::abs(matrix.m[6]) <= eps &&
           std::abs(matrix.m[7]) <= eps &&
           std::abs(matrix.m[8] - 1.f) <= eps;
}

Rect offsetRect(Rect rect, Point offset) noexcept {
    rect.x += offset.x;
    rect.y += offset.y;
    return rect;
}

bool canReplayPreparedLeaf(SceneNode const& node) {
    if (node.kind() == SceneNodeKind::Group || !node.canPrepareRenderOps() || !node.children().empty()) {
        return false;
    }
    return node.kind() != SceneNodeKind::Rect ||
           !static_cast<RectNode const&>(node).clipsContents();
}

bool subtreeContainsRasterCache(SceneNode const& node) {
    for (std::unique_ptr<SceneNode> const& child : node.children()) {
        if (child->kind() == SceneNodeKind::RasterCache || subtreeContainsRasterCache(*child)) {
            return true;
        }
    }
    return false;
}

bool canReplayPreparedGroup(SceneNode const& node) {
    Rect const bounds = node.localBounds();
    return node.kind() == SceneNodeKind::Group &&
           node.layoutFlow() == LayoutFlow::None &&
           !node.children().empty() &&
           bounds.width > 0.f &&
           bounds.height > 0.f &&
           isIdentityTransform(node.transform()) &&
           !subtreeContainsRasterCache(node);
}

enum class RenderTraversalMode : std::uint8_t {
    Normal,
    PreparedCacheBypass,
};

float rasterCacheDpiScaleForCanvas(Canvas* canvas) noexcept {
#if defined(FLUX_PLATFORM_MACOS)
    return dpiScaleForCanvas(canvas);
#else
    (void)canvas;
    return 1.f;
#endif
}

#if defined(FLUX_PLATFORM_MACOS)
MetalRecorderSlice fullRecordedSlice(MetalFrameRecorder const &recorded) {
    return MetalRecorderSlice {
        .orderStart = 0,
        .orderCount = static_cast<std::uint32_t>(recorded.opOrder.size()),
        .rectStart = 0,
        .rectCount = static_cast<std::uint32_t>(recorded.rectOps.size()),
        .imageStart = 0,
        .imageCount = static_cast<std::uint32_t>(recorded.imageOps.size()),
        .pathOpStart = 0,
        .pathOpCount = static_cast<std::uint32_t>(recorded.pathOps.size()),
        .glyphOpStart = 0,
        .glyphOpCount = static_cast<std::uint32_t>(recorded.glyphOps.size()),
        .pathVertexStart = 0,
        .pathVertexCount = static_cast<std::uint32_t>(recorded.pathVerts.size()),
        .glyphVertexStart = 0,
        .glyphVertexCount = recorded.glyphVertexCount,
    };
}
#endif

#if defined(FLUX_PLATFORM_MACOS)
bool roundedClipHasEntries(MetalRoundedClipStack const &clip) noexcept {
    return clip.header.x > 0.f;
}

template <typename Op>
bool opHasRecordedClip(Op const &op) noexcept {
    return op.scissorValid || roundedClipHasEntries(op.roundedClip);
}

bool recordedOpsContainClipState(MetalFrameRecorder const &recorded) noexcept {
    return std::any_of(recorded.rectOps.begin(), recorded.rectOps.end(), opHasRecordedClip<MetalRectOp>) ||
           std::any_of(recorded.imageOps.begin(), recorded.imageOps.end(), opHasRecordedClip<MetalImageOp>) ||
           std::any_of(recorded.pathOps.begin(), recorded.pathOps.end(), opHasRecordedClip<MetalPathOp>) ||
           std::any_of(recorded.glyphOps.begin(), recorded.glyphOps.end(), opHasRecordedClip<MetalGlyphOp>);
}
#endif

class CanvasRenderer final : public Renderer {
  public:
    explicit CanvasRenderer(Canvas &canvas) : canvas_(canvas) {}

    void save() override { canvas_.save(); }
    void restore() override { canvas_.restore(); }
    void translate(Point offset) override { canvas_.translate(offset); }
    void transform(Mat3 const &matrix) override { canvas_.transform(matrix); }
    void clipRect(Rect rect, CornerRadius const &cornerRadius, bool antiAlias) override {
        canvas_.clipRect(rect, cornerRadius, antiAlias);
    }
    bool quickReject(Rect rect) const override { return canvas_.quickReject(rect); }
    void setOpacity(float opacity) override { canvas_.setOpacity(opacity); }
    void setBlendMode(BlendMode mode) override { canvas_.setBlendMode(mode); }
    void drawRect(Rect const &rect, CornerRadius const &cornerRadius, FillStyle const &fill, StrokeStyle const &stroke, ShadowStyle const &shadow) override {
        canvas_.drawRect(rect, cornerRadius, fill, stroke, shadow);
    }
    void drawPath(Path const &path, FillStyle const &fill, StrokeStyle const &stroke, ShadowStyle const &shadow) override {
        canvas_.drawPath(path, fill, stroke, shadow);
    }
    void drawTextLayout(TextLayout const &layout) override { canvas_.drawTextLayout(layout, Point {}); }
    void drawImage(Image const &image, Rect const &bounds, ImageFillMode fillMode) override {
        canvas_.drawImage(image, bounds, fillMode, CornerRadius {}, 1.f);
    }
    std::unique_ptr<PreparedRenderOps> prepare(SceneNode const &node) override;
    Canvas *canvas() noexcept override { return &canvas_; }

  private:
    Canvas &canvas_;
};

#if defined(FLUX_PLATFORM_MACOS)
class CanvasPreparedRenderOps final : public PreparedRenderOps {
  public:
    explicit CanvasPreparedRenderOps(MetalFrameRecorder recorded) : recorded_(std::move(recorded)), slice_(fullRecordedSlice(recorded_)) {}

    bool replay(Renderer &renderer) const override {
        return replayRecordedLocalOpsForCanvas(renderer.canvas(), recorded_, slice_);
    }

  private:
    MetalFrameRecorder recorded_;
    MetalRecorderSlice slice_ {};
};
#endif

class CanvasUnreplayablePreparedRenderOps final : public PreparedRenderOps {
  public:
    bool replay(Renderer &) const override {
        return false;
    }
};

std::unique_ptr<PreparedRenderOps> CanvasRenderer::prepare(SceneNode const &node) {
#if defined(FLUX_PLATFORM_MACOS)
    MetalFrameRecorder recorded;
    if (!beginRecordedOpsCaptureForCanvas(&canvas_, &recorded)) {
        return nullptr;
    }
    node.render(*this);
    endRecordedOpsCaptureForCanvas(&canvas_);
    if (recordedOpsContainClipState(recorded)) {
        // Local replay retags cached ops with the caller's current clip. Until it can merge
        // recorded and caller clips, keep internally clipped leaves on the live render path.
        return std::make_unique<CanvasUnreplayablePreparedRenderOps>();
    }
    return std::make_unique<CanvasPreparedRenderOps>(std::move(recorded));
#else
    (void)node;
    return nullptr;
#endif
}

} // namespace

struct SceneRenderer::Impl {
    explicit Impl(Canvas &canvas) : renderer(nullptr), ownedRenderer(std::make_unique<CanvasRenderer>(canvas)) {
        renderer = ownedRenderer.get();
    }

    explicit Impl(Renderer &rendererValue) : renderer(&rendererValue) {}

    void render(SceneGraph const &graph) {
        render(graph.root());
    }

    void render(SceneNode const &node) {
        debug::perf::recordSceneRenderPass();
        if (kEnablePreparedRenderCache) {
            prepareNodeCache(node);
        }
        renderNode(node, 1.f, Point {});
    }

    void prepareNodeCache(SceneNode const &node) {
        if (!kEnablePreparedRenderCache) {
            return;
        }
        if (!detail::SceneNodeAccess::subtreeDirty(node)) {
            return;
        }
        if (node.kind() == SceneNodeKind::RasterCache) {
            const_cast<RasterCacheNode&>(static_cast<RasterCacheNode const&>(node)).invalidateCache();
            detail::SceneNodeAccess::preparedRenderOps(node).reset();
            if (detail::SceneNodeAccess::ownPaintingDirty(node)) {
                detail::SceneNodeAccess::clearDirty(node);
            }
            detail::SceneNodeAccess::clearSubtreeDirty(node);
            return;
        }
        bool const hadPreparedGroup =
            node.kind() == SceneNodeKind::Group &&
            static_cast<bool>(detail::SceneNodeAccess::preparedRenderOps(node));
        if (node.kind() != SceneNodeKind::Group && node.canPrepareRenderOps()) {
            std::unique_ptr<PreparedRenderOps> &prepared =
                detail::SceneNodeAccess::preparedRenderOps(node);
            if (detail::SceneNodeAccess::ownPaintingDirty(node) || !prepared) {
                debug::perf::recordPreparedPrepareCall();
                prepared = renderer->prepare(node);
                detail::SceneNodeAccess::clearDirty(node);
            }
        } else if (node.kind() != SceneNodeKind::Group) {
            detail::SceneNodeAccess::preparedRenderOps(node).reset();
            if (detail::SceneNodeAccess::ownPaintingDirty(node)) {
                detail::SceneNodeAccess::clearDirty(node);
            }
        } else {
            std::unique_ptr<PreparedRenderOps> &prepared =
                detail::SceneNodeAccess::preparedRenderOps(node);
            if (prepared) {
                // A previously cached group that becomes dirty is probably on an animated path.
                // Drop the broad cache and let stable child groups continue replaying independently.
                prepared.reset();
                detail::SceneNodeAccess::suppressPreparedGroupCache(node);
            }
            if (detail::SceneNodeAccess::ownPaintingDirty(node)) {
                detail::SceneNodeAccess::clearDirty(node);
            }
        }

        for (std::unique_ptr<SceneNode> const &child : node.children()) {
            prepareNodeCache(*child);
        }

        if (node.kind() == SceneNodeKind::Group && !hadPreparedGroup &&
            !detail::SceneNodeAccess::preparedGroupCacheSuppressed(node) &&
            !detail::SceneNodeAccess::preparedRenderOps(node) &&
            canReplayPreparedGroup(node)) {
            debug::perf::recordPreparedPrepareCall();
            detail::SceneNodeAccess::preparedRenderOps(node) = prepareSubtree(node);
        }

        detail::SceneNodeAccess::clearSubtreeDirty(node);
    }

    bool renderRasterCacheNode(RasterCacheNode const& node, float nodeOpacity,
                               Point accumulatedTranslation) {
        Canvas *canvas = renderer->canvas();
        if (!canvas) {
            return false;
        }
        Size const logicalSize = node.size();
        if (logicalSize.width <= 0.f || logicalSize.height <= 0.f) {
            return true;
        }
        float const dpiScale = rasterCacheDpiScaleForCanvas(canvas);
        std::shared_ptr<Image> cached =
            node.hasValidCache(logicalSize, dpiScale) ? node.cachedImage() : nullptr;
        if (!cached) {
            cached = rasterizeToImage(*canvas, logicalSize, [this, &node](Canvas&, Rect) {
                for (std::unique_ptr<SceneNode> const &child : node.children()) {
                    renderNode(*child, 1.f, Point {}, false, RenderTraversalMode::PreparedCacheBypass);
                }
            }, dpiScale);
            if (!cached) {
                return false;
            }
            node.setCachedImage(cached, logicalSize, dpiScale);
            node.noteRasterized();
        }

        renderer->save();
        if (!isZeroOffset(accumulatedTranslation)) {
            renderer->translate(accumulatedTranslation);
        }
        if (!isIdentityTransform(node.transform())) {
            renderer->transform(node.transform());
        }
        renderer->setOpacity(nodeOpacity);
        renderer->drawImage(*cached, Rect::sharp(0.f, 0.f, logicalSize.width, logicalSize.height),
                            ImageFillMode::Stretch);
        renderer->restore();
        detail::SceneNodeAccess::clearDirty(node);
        detail::SceneNodeAccess::clearSubtreeDirty(node);
        return true;
    }

    std::unique_ptr<PreparedRenderOps> prepareSubtree(SceneNode const &node) {
        Canvas *canvas = renderer->canvas();
        if (!canvas) {
            return nullptr;
        }
#if !defined(FLUX_PLATFORM_MACOS)
        (void)node;
        return nullptr;
#else
        MetalFrameRecorder recorded;
        if (!beginRecordedOpsCaptureForCanvas(canvas, &recorded)) {
            return nullptr;
        }
        for (std::unique_ptr<SceneNode> const &child : node.children()) {
            renderNode(*child, 1.f, Point {}, false, RenderTraversalMode::PreparedCacheBypass);
        }
        endRecordedOpsCaptureForCanvas(canvas);
        if (recordedOpsContainClipState(recorded)) {
            // Group captures include clips from descendants. Replaying them as one local
            // display list would drop those nested clips, so let the normal traversal render it.
            return std::make_unique<CanvasUnreplayablePreparedRenderOps>();
        }
        return std::make_unique<CanvasPreparedRenderOps>(std::move(recorded));
#endif
    }

    void renderNode(SceneNode const &node, float inheritedOpacity, Point inheritedTranslation,
                    bool collectCounters = true,
                    RenderTraversalMode mode = RenderTraversalMode::Normal) {
        bool const usePreparedCache =
            mode == RenderTraversalMode::Normal && node.transform().isTranslationOnly();
        if (collectCounters) {
            debug::perf::recordSceneNodeVisit(node.kind() == SceneNodeKind::Group);
        }
        float nodeOpacity = inheritedOpacity;
        if (node.kind() == SceneNodeKind::Rect) {
            nodeOpacity *= static_cast<RectNode const &>(node).opacity();
        }

        Rect const bounds = node.bounds();
        Point const nodePosition {bounds.x, bounds.y};
        Point const accumulatedTranslation = inheritedTranslation + nodePosition;
        Rect const localBounds = node.localBounds();
        bool const clipsContents =
            node.kind() == SceneNodeKind::Rect &&
            static_cast<RectNode const &>(node).clipsContents();

        if (node.kind() == SceneNodeKind::RasterCache &&
            renderRasterCacheNode(static_cast<RasterCacheNode const&>(node), nodeOpacity,
                                  accumulatedTranslation)) {
            return;
        }

        if (node.kind() == SceneNodeKind::Group && usePreparedCache &&
            kEnablePreparedRenderCache && !detail::SceneNodeAccess::subtreeDirty(node) &&
            canReplayPreparedGroup(node)) {
            if (localBounds.width > 0.f && localBounds.height > 0.f &&
                renderer->quickReject(offsetRect(localBounds, accumulatedTranslation))) {
                if (collectCounters) {
                    debug::perf::recordSceneQuickReject();
                }
                return;
            }
            std::unique_ptr<PreparedRenderOps> &prepared =
                detail::SceneNodeAccess::preparedRenderOps(node);
            if (prepared) {
                bool const needsState = !isZeroOffset(accumulatedTranslation) || nodeOpacity != 1.f;
                if (needsState) {
                    renderer->save();
                    if (!isZeroOffset(accumulatedTranslation)) {
                        renderer->translate(accumulatedTranslation);
                    }
                    if (nodeOpacity != 1.f) {
                        renderer->setOpacity(nodeOpacity);
                    }
                }
                if (collectCounters) {
                    debug::perf::recordPreparedReplayCall();
                }
                bool const replayed = prepared->replay(*renderer);
                if (collectCounters) {
                    debug::perf::recordPreparedReplayResult(replayed);
                }
                if (needsState) {
                    renderer->restore();
                }
                if (replayed) {
                    return;
                }
            }
        }

        if (node.kind() == SceneNodeKind::Group &&
            !clipsContents &&
            isIdentityTransform(node.transform())) {
            if (localBounds.width > 0.f && localBounds.height > 0.f &&
                renderer->quickReject(offsetRect(localBounds, accumulatedTranslation))) {
                if (collectCounters) {
                    debug::perf::recordSceneQuickReject();
                }
                return;
            }
            for (std::unique_ptr<SceneNode> const &child : node.children()) {
                renderNode(*child, nodeOpacity, accumulatedTranslation, collectCounters, mode);
            }
            return;
        }

        if (node.kind() != SceneNodeKind::Group && usePreparedCache && kEnablePreparedRenderCache &&
            canReplayPreparedLeaf(node)) {
            if (localBounds.width > 0.f && localBounds.height > 0.f) {
                Rect const translatedBounds = detail::transformBounds(
                    Mat3::translate(accumulatedTranslation) * node.transform(), localBounds);
                if (renderer->quickReject(translatedBounds)) {
                    if (collectCounters) {
                        debug::perf::recordSceneQuickReject();
                    }
                    return;
                }
            }

            std::unique_ptr<PreparedRenderOps> &prepared =
                detail::SceneNodeAccess::preparedRenderOps(node);
            auto replayPrepared = [&]() {
                if (!prepared) {
                    return false;
                }
                bool const needsState = !isZeroOffset(accumulatedTranslation) ||
                                        !isIdentityTransform(node.transform()) ||
                                        nodeOpacity != 1.f;
                if (needsState) {
                    renderer->save();
                    if (!isZeroOffset(accumulatedTranslation)) {
                        renderer->translate(accumulatedTranslation);
                    }
                    if (!isIdentityTransform(node.transform())) {
                        renderer->transform(node.transform());
                    }
                    if (nodeOpacity != 1.f) {
                        renderer->setOpacity(nodeOpacity);
                    }
                }
                if (collectCounters) {
                    debug::perf::recordPreparedReplayCall();
                }
                bool const replayed = prepared->replay(*renderer);
                if (collectCounters) {
                    debug::perf::recordPreparedReplayResult(replayed);
                }
                if (needsState) {
                    renderer->restore();
                }
                return replayed;
            };
            if (replayPrepared()) {
                return;
            }
        }

        renderer->save();
        renderer->translate(accumulatedTranslation);
        renderer->transform(node.transform());

        renderer->setOpacity(nodeOpacity);

        if (localBounds.width > 0.f && localBounds.height > 0.f &&
            renderer->quickReject(localBounds)) {
            if (collectCounters) {
                debug::perf::recordSceneQuickReject();
            }
            renderer->restore();
            return;
        }

        if (node.kind() != SceneNodeKind::Group) {
            if (!usePreparedCache || !kEnablePreparedRenderCache || !node.canPrepareRenderOps()) {
                if (collectCounters) {
                    debug::perf::recordLiveLeafRender();
                }
                node.render(*renderer);
            } else {
                std::unique_ptr<PreparedRenderOps> &prepared =
                    detail::SceneNodeAccess::preparedRenderOps(node);
                if (!prepared) {
                    if (collectCounters) {
                        debug::perf::recordLiveLeafRender();
                    }
                    node.render(*renderer);
                } else {
                    if (collectCounters) {
                        debug::perf::recordPreparedReplayCall();
                    }
                    bool const replayed = prepared->replay(*renderer);
                    if (collectCounters) {
                        debug::perf::recordPreparedReplayResult(replayed);
                    }
                    if (!replayed) {
                        if (collectCounters) {
                            debug::perf::recordLiveLeafRender();
                        }
                        node.render(*renderer);
                    }
                }
            }
        }

        if (clipsContents) {
            RectNode const &rectNode = static_cast<RectNode const &>(node);
            renderer->save();
            renderer->clipRect(Rect::sharp(0.f, 0.f, node.size().width, node.size().height),
                               rectNode.cornerRadius());
        }

        RenderTraversalMode const childMode =
            mode == RenderTraversalMode::PreparedCacheBypass ||
                    node.kind() == SceneNodeKind::RasterCache ||
                    !node.transform().isTranslationOnly()
                ? RenderTraversalMode::PreparedCacheBypass
                : mode;
        for (std::unique_ptr<SceneNode> const &child : node.children()) {
            renderNode(*child, nodeOpacity, Point {}, collectCounters, childMode);
        }

        if (clipsContents) {
            renderer->restore();
        }
        renderer->restore();
    }

    Renderer *renderer = nullptr;
    std::unique_ptr<Renderer> ownedRenderer;
};

SceneRenderer::SceneRenderer(Canvas &canvas) : impl_(std::make_unique<Impl>(canvas)) {}

SceneRenderer::SceneRenderer(Renderer &renderer) : impl_(std::make_unique<Impl>(renderer)) {}

SceneRenderer::~SceneRenderer() = default;

void SceneRenderer::render(SceneGraph const &graph) {
    debug::perf::ScopedTimer perfTimer(debug::perf::TimedMetric::SceneRender);
    impl_->render(graph);
}

void SceneRenderer::render(SceneNode const &node) {
    debug::perf::ScopedTimer perfTimer(debug::perf::TimedMetric::SceneRender);
    impl_->render(node);
}

} // namespace flux::scenegraph
