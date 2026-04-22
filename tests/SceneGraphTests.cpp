#include <doctest/doctest.h>

#include <Flux/SceneGraph/Renderer.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneRenderer.hpp>

#include <memory>
#include <vector>

namespace {

using namespace flux;
using namespace flux::scenegraph;

class RecordingRenderer final : public Renderer {
public:
  struct RectDraw {
    Rect rect{};
    Point translation{};
  };

  struct TextDraw {
    Point origin{};
    Point translation{};
  };

  std::vector<RectDraw> rectDraws;
  std::vector<TextDraw> textDraws;

  void save() override { translations_.push_back(translations_.back()); }
  void restore() override {
    if (translations_.size() > 1) {
      translations_.pop_back();
    }
  }

  void translate(Point offset) override { translations_.back() = translations_.back() + offset; }
  void transform(Mat3 const&) override {}
  void clipRect(Rect, CornerRadius const&, bool) override {}
  bool quickReject(Rect) const override { return false; }
  void setOpacity(float) override {}
  void setBlendMode(BlendMode) override {}

  void drawRect(Rect const& rect, CornerRadius const&, FillStyle const&, StrokeStyle const&, ShadowStyle const&) override {
    rectDraws.push_back({rect, translations_.back()});
  }

  void drawLine(Point, Point, StrokeStyle const&) override {}

  void drawPath(Path const&, FillStyle const&, StrokeStyle const&, ShadowStyle const&) override {}

  void drawTextLayout(TextLayout const&, Point origin) override {
    textDraws.push_back({origin, translations_.back()});
  }

  void drawImage(Image const&, Rect const&, ImageFillMode, CornerRadius const&, float) override {}

private:
  std::vector<Point> translations_ {Point{}};
};

TEST_CASE("SceneRenderer accumulates parent-space bounds as local translations") {
  auto root = std::make_unique<GroupNode>(Rect{10.f, 20.f, 300.f, 200.f});
  auto panel = std::make_unique<GroupNode>(Rect{15.f, 25.f, 120.f, 80.f});
  panel->appendChild(std::make_unique<RectNode>(Rect{5.f, 6.f, 100.f, 50.f}, FillStyle::solid(Colors::red)));
  root->appendChild(std::move(panel));

  SceneGraph graph{std::move(root)};
  RecordingRenderer renderer;
  SceneRenderer sceneRenderer{renderer};

  sceneRenderer.render(graph);

  REQUIRE(renderer.rectDraws.size() == 1);
  CHECK(renderer.rectDraws[0].translation == Point{30.f, 51.f});
  CHECK(renderer.rectDraws[0].rect == Rect::sharp(0.f, 0.f, 100.f, 50.f));
}

TEST_CASE("TextNode renders its stored layout at the node-local origin") {
  auto layout = std::make_shared<TextLayout>();
  layout->measuredSize = Size{80.f, 20.f};

  auto root = std::make_unique<GroupNode>(Rect{0.f, 0.f, 200.f, 100.f});
  root->appendChild(std::make_unique<TextNode>(Rect{12.f, 18.f, 80.f, 20.f}, layout, Point{4.f, 6.f}));

  SceneGraph graph{std::move(root)};
  RecordingRenderer renderer;
  SceneRenderer sceneRenderer{renderer};

  sceneRenderer.render(graph);

  REQUIRE(renderer.textDraws.size() == 1);
  CHECK(renderer.textDraws[0].translation == Point{12.f, 18.f});
  CHECK(renderer.textDraws[0].origin == Point{4.f, 6.f});
}

} // namespace
