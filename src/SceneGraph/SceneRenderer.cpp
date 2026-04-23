#include <Flux/SceneGraph/SceneRenderer.hpp>

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/SceneGraph/PathNode.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/Renderer.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>

#include "Graphics/Metal/MetalCanvas.hpp"
#include "Graphics/Metal/MetalCanvasTypes.hpp"
#include "Graphics/Metal/MetalFrameRecorder.hpp"
#include "SceneGraph/SceneNodeInternal.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>

namespace flux::scenegraph {

namespace {

constexpr bool kEnablePreparedRenderCache = true;

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
        .glyphVertexCount = static_cast<std::uint32_t>(recorded.glyphVerts.size()),
    };
}

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
    void drawImage(Image const &image, Rect const &bounds) override {
        canvas_.drawImage(image, bounds, ImageFillMode::Cover, CornerRadius {}, 1.f);
    }
    std::unique_ptr<PreparedRenderOps> prepare(SceneNode const &node) override;
    Canvas *canvas() noexcept override { return &canvas_; }

  private:
    Canvas &canvas_;
};

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

std::unique_ptr<PreparedRenderOps> CanvasRenderer::prepare(SceneNode const &node) {
    MetalFrameRecorder recorded;
    if (!beginRecordedOpsCaptureForCanvas(&canvas_, &recorded)) {
        return nullptr;
    }
    node.render(*this);
    endRecordedOpsCaptureForCanvas(&canvas_);
    return std::make_unique<CanvasPreparedRenderOps>(std::move(recorded));
}

} // namespace

struct SceneRenderer::Impl {
    struct CacheEntry {
        std::unique_ptr<PreparedRenderOps> prepared;
        std::uint64_t lastVisitedEpoch = 0;
    };

    explicit Impl(Canvas &canvas) : renderer(nullptr), ownedRenderer(std::make_unique<CanvasRenderer>(canvas)) {
        renderer = ownedRenderer.get();
    }

    explicit Impl(Renderer &rendererValue) : renderer(&rendererValue) {}

    void render(SceneGraph const &graph) {
        render(graph.root());
    }

    void render(SceneNode const &node) {
        if (kEnablePreparedRenderCache) {
            if (node.kind() == SceneNodeKind::Group && node.isDirty()) {
                cache.clear();
            }
            prepareNodeCache(node);
        }
        ++renderEpoch;
        renderNode(node, 1.f);
        if (kEnablePreparedRenderCache) {
            std::erase_if(cache, [this](auto const &entry) {
                return entry.second.lastVisitedEpoch != renderEpoch;
            });
        }
    }

    void prepareNodeCache(SceneNode const &node) {
        if (!kEnablePreparedRenderCache) {
            return;
        }
        if (node.kind() != SceneNodeKind::Group) {
            CacheEntry &entry = cache[&node];
            if (node.isDirty() || !entry.prepared) {
                entry.prepared = renderer->prepare(node);
                detail::SceneNodeAccess::clearDirty(node);
            }
        } else if (node.isDirty()) {
            detail::SceneNodeAccess::clearDirty(node);
        }

        for (std::unique_ptr<SceneNode> const &child : node.children()) {
            prepareNodeCache(*child);
        }
    }

    void renderNode(SceneNode const &node, float inheritedOpacity) {
        if (kEnablePreparedRenderCache) {
            if (auto it = cache.find(&node); it != cache.end()) {
                it->second.lastVisitedEpoch = renderEpoch;
            }
        }

        renderer->save();
        Rect const bounds = node.bounds();
        renderer->translate(Point {bounds.x, bounds.y});
        renderer->transform(node.transform());

        float nodeOpacity = inheritedOpacity;
        if (node.kind() == SceneNodeKind::Rect) {
            nodeOpacity *= static_cast<RectNode const &>(node).opacity();
        }
        renderer->setOpacity(nodeOpacity);

        Rect const localBounds = node.localBounds();
        if (localBounds.width > 0.f && localBounds.height > 0.f &&
            renderer->quickReject(localBounds)) {
            for (std::unique_ptr<SceneNode> const &child : node.children()) {
                markSubtreeVisited(*child);
            }
            renderer->restore();
            return;
        }

        if (node.kind() != SceneNodeKind::Group) {
            if (!kEnablePreparedRenderCache) {
                node.render(*renderer);
            } else {
                CacheEntry &entry = cache[&node];
                entry.lastVisitedEpoch = renderEpoch;
                if (!entry.prepared || !entry.prepared->replay(*renderer)) {
                    node.render(*renderer);
                }
            }
        }

        bool const clipsContents =
            node.kind() == SceneNodeKind::Rect &&
            static_cast<RectNode const &>(node).clipsContents();
        if (clipsContents) {
            RectNode const &rectNode = static_cast<RectNode const &>(node);
            renderer->save();
            renderer->clipRect(node.localBounds(), rectNode.cornerRadius());
        }

        for (std::unique_ptr<SceneNode> const &child : node.children()) {
            renderNode(*child, nodeOpacity);
        }

        if (clipsContents) {
            renderer->restore();
        }
        renderer->restore();
    }

    void markSubtreeVisited(SceneNode const &node) {
        if (!kEnablePreparedRenderCache) {
            return;
        }
        if (auto it = cache.find(&node); it != cache.end()) {
            it->second.lastVisitedEpoch = renderEpoch;
        }
        for (std::unique_ptr<SceneNode> const &child : node.children()) {
            markSubtreeVisited(*child);
        }
    }

    Renderer *renderer = nullptr;
    std::unique_ptr<Renderer> ownedRenderer;
    std::unordered_map<SceneNode const *, CacheEntry> cache;
    std::uint64_t renderEpoch = 0;
};

SceneRenderer::SceneRenderer(Canvas &canvas) : impl_(std::make_unique<Impl>(canvas)) {}

SceneRenderer::SceneRenderer(Renderer &renderer) : impl_(std::make_unique<Impl>(renderer)) {}

SceneRenderer::~SceneRenderer() = default;

void SceneRenderer::render(SceneGraph const &graph) {
    impl_->render(graph);
}

void SceneRenderer::render(SceneNode const &node) {
    impl_->render(node);
}

} // namespace flux::scenegraph
