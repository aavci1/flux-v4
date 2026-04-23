#include <doctest/doctest.h>

#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/Renderer.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneRenderer.hpp>
#include <Flux/SceneGraph/TextNode.hpp>

#include <memory>
#include <vector>

namespace {

using namespace flux;
using namespace flux::scenegraph;

class RecordingRenderer final : public Renderer {
  public:
    struct RectDraw {
        Rect rect {};
        Point translation {};
    };

    struct ClipDraw {
        Rect rect {};
        CornerRadius cornerRadius {};
        Point translation {};
    };

    std::vector<RectDraw> rectDraws;
    std::vector<Point> textTranslations;
    std::vector<ClipDraw> clipDraws;

    void save() override { translations_.push_back(translations_.back()); }
    void restore() override {
        if (translations_.size() > 1) {
            translations_.pop_back();
        }
    }

    void translate(Point offset) override { translations_.back() = translations_.back() + offset; }
    void transform(Mat3 const &) override {}
    void clipRect(Rect rect, CornerRadius const &cornerRadius, bool) override {
        clipDraws.push_back({rect, cornerRadius, translations_.back()});
    }
    bool quickReject(Rect) const override { return false; }
    void setOpacity(float) override {}
    void setBlendMode(BlendMode) override {}

    void drawRect(Rect const &rect, CornerRadius const &, FillStyle const &, StrokeStyle const &, ShadowStyle const &) override {
        rectDraws.push_back({rect, translations_.back()});
    }

    void drawTextLayout(TextLayout const &) override { textTranslations.push_back(translations_.back()); }

    void drawImage(Image const &, Rect const &) override {}

  private:
    std::vector<Point> translations_ {Point {}};
};

class PreparedCountingRenderer final : public Renderer {
  public:
    struct PreparedMarker final : public PreparedRenderOps {
        explicit PreparedMarker(PreparedCountingRenderer &owner)
            : owner_(owner) {}

        bool replay(Renderer &) const override {
            ++owner_.replayCalls;
            return true;
        }

      private:
        PreparedCountingRenderer &owner_;
    };

    int prepareCalls = 0;
    int replayCalls = 0;
    int fallbackRectDraws = 0;

    void save() override {}
    void restore() override {}
    void translate(Point) override {}
    void transform(Mat3 const &) override {}
    void clipRect(Rect, CornerRadius const &, bool) override {}
    bool quickReject(Rect) const override { return false; }
    void setOpacity(float) override {}
    void setBlendMode(BlendMode) override {}

    void drawRect(Rect const &, CornerRadius const &, FillStyle const &, StrokeStyle const &, ShadowStyle const &) override {
        ++fallbackRectDraws;
    }

    void drawTextLayout(TextLayout const &) override {}
    void drawImage(Image const &, Rect const &) override {}

    std::unique_ptr<PreparedRenderOps> prepare(SceneNode const &) override {
        ++prepareCalls;
        return std::make_unique<PreparedMarker>(*this);
    }
};

TEST_CASE("SceneRenderer accumulates parent-space bounds as local translations") {
    auto root = std::make_unique<GroupNode>(Rect {10.f, 20.f, 300.f, 200.f});
    auto panel = std::make_unique<GroupNode>(Rect {15.f, 25.f, 120.f, 80.f});
    panel->appendChild(std::make_unique<RectNode>(Rect {5.f, 6.f, 100.f, 50.f}, FillStyle::solid(Colors::red)));
    root->appendChild(std::move(panel));

    SceneGraph graph {std::move(root)};
    RecordingRenderer renderer;
    SceneRenderer sceneRenderer {renderer};

    sceneRenderer.render(graph);

    REQUIRE(renderer.rectDraws.size() == 1);
    CHECK(renderer.rectDraws[0].translation == Point {30.f, 51.f});
    CHECK(renderer.rectDraws[0].rect == Rect::sharp(0.f, 0.f, 100.f, 50.f));
}

TEST_CASE("TextNode renders its stored layout in node-local space") {
    auto layout = std::make_shared<TextLayout>();
    layout->measuredSize = Size {80.f, 20.f};

    auto root = std::make_unique<GroupNode>(Rect {0.f, 0.f, 200.f, 100.f});
    root->appendChild(std::make_unique<TextNode>(Rect {12.f, 18.f, 80.f, 20.f}, layout));

    SceneGraph graph {std::move(root)};
    RecordingRenderer renderer;
    SceneRenderer sceneRenderer {renderer};

    sceneRenderer.render(graph);

    REQUIRE(renderer.textTranslations.size() == 1);
    CHECK(renderer.textTranslations[0] == Point {12.f, 18.f});
}

TEST_CASE("RectNode clipping scopes child traversal to the node bounds") {
    auto layout = std::make_shared<TextLayout>();
    layout->measuredSize = Size {40.f, 16.f};

    auto root = std::make_unique<GroupNode>(Rect {0.f, 0.f, 200.f, 120.f});
    auto container = std::make_unique<RectNode>(
        Rect {10.f, 20.f, 90.f, 60.f},
        FillStyle::none(),
        StrokeStyle::none(),
        CornerRadius {6.f}
    );
    container->setClipsContents(true);
    container->appendChild(std::make_unique<TextNode>(Rect {5.f, 7.f, 40.f, 16.f}, layout));
    root->appendChild(std::move(container));

    SceneGraph graph {std::move(root)};
    RecordingRenderer renderer;
    SceneRenderer sceneRenderer {renderer};

    sceneRenderer.render(graph);

    REQUIRE(renderer.clipDraws.size() == 1);
    CHECK(renderer.clipDraws[0].translation == Point {10.f, 20.f});
    CHECK(renderer.clipDraws[0].rect == Rect::sharp(0.f, 0.f, 90.f, 60.f));
    CHECK(renderer.clipDraws[0].cornerRadius == CornerRadius {6.f});
    REQUIRE(renderer.textTranslations.size() == 1);
    CHECK(renderer.textTranslations[0] == Point {15.f, 27.f});
}

TEST_CASE("SceneRenderer reuses prepared ops for position-only changes and rebuilds on content changes") {
    auto root = std::make_unique<GroupNode>(Rect {0.f, 0.f, 200.f, 100.f});
    auto rect = std::make_unique<RectNode>(Rect {10.f, 12.f, 50.f, 30.f}, FillStyle::solid(Colors::red));
    RectNode *rectNode = rect.get();
    root->appendChild(std::move(rect));

    SceneGraph graph {std::move(root)};
    PreparedCountingRenderer renderer;
    SceneRenderer sceneRenderer {renderer};

    sceneRenderer.render(graph);
    CHECK(renderer.prepareCalls == 1);
    CHECK(renderer.replayCalls == 1);
    CHECK(renderer.fallbackRectDraws == 0);

    rectNode->setPosition(Point {40.f, 24.f});
    sceneRenderer.render(graph);
    CHECK(renderer.prepareCalls == 1);
    CHECK(renderer.replayCalls == 2);
    CHECK(renderer.fallbackRectDraws == 0);

    rectNode->setClipsContents(true);
    sceneRenderer.render(graph);
    CHECK(renderer.prepareCalls == 1);
    CHECK(renderer.replayCalls == 3);
    CHECK(renderer.fallbackRectDraws == 0);

    rectNode->setFill(FillStyle::solid(Colors::blue));
    sceneRenderer.render(graph);
    CHECK(renderer.prepareCalls == 2);
    CHECK(renderer.replayCalls == 4);
    CHECK(renderer.fallbackRectDraws == 0);

    rectNode->setSize(Size {64.f, 30.f});
    sceneRenderer.render(graph);
    CHECK(renderer.prepareCalls == 3);
    CHECK(renderer.replayCalls == 5);
    CHECK(renderer.fallbackRectDraws == 0);
}

} // namespace
