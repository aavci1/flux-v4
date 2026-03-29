#include <Flux.hpp>
#include <Flux/Core/Application.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/UI.hpp>

#include <algorithm>
#include <array>
#include <vector>

using namespace flux;

namespace {

struct DemoCell {
  BlendMode mode;
  const char* title;
};

// Row-major grid: each cell shows cyan + magenta circles; second circle uses `mode`.
constexpr std::array<DemoCell, 12> kGrid{{
    {BlendMode::Normal, "Normal"},
    {BlendMode::Multiply, "Multiply"},
    {BlendMode::Screen, "Screen"},
    {BlendMode::Darken, "Darken"},
    {BlendMode::Lighten, "Lighten"},
    {BlendMode::DstOver, "DstOver"},
    {BlendMode::SrcIn, "SrcIn"},
    {BlendMode::DstIn, "DstIn"},
    {BlendMode::SrcOut, "SrcOut"},
    {BlendMode::DstOut, "DstOut"},
    {BlendMode::Src, "Src"},
    {BlendMode::Dst, "Dst"},
}};

struct BlendCell {
  static constexpr int kCols = 4;
  static constexpr int kRows = 3;
  static constexpr float kMargin = 24.f;
  static constexpr float kGap = 10.f;
  /// Default `layoutHint` when parent does not set one (matches demo window defaults).
  static constexpr float kDefaultWinW = 920.f;
  static constexpr float kDefaultWinH = 700.f;

  BlendMode mode = BlendMode::Normal;
  const char* title = "";
  /// Used when `measure` sees unbounded constraints (HStack does not propagate width to children).
  /// Parent should set this to the current window / content size so the grid fills the layout.
  Size layoutHint{kDefaultWinW, kDefaultWinH};

  Size measure(LayoutConstraints const& c) const {
    float const winW = std::isfinite(c.maxWidth) && c.maxWidth > 0.f ? c.maxWidth : layoutHint.width;
    float const winH = std::isfinite(c.maxHeight) && c.maxHeight > 0.f ? c.maxHeight : layoutHint.height;
    float const innerW = std::max(0.f, winW - 2.f * kMargin);
    float const innerH = std::max(0.f, winH - 2.f * kMargin);
    float const cellW =
        std::max(0.f, (innerW - kGap * (static_cast<float>(kCols) - 1.f)) / static_cast<float>(kCols));
    float const cellH =
        std::max(0.f, (innerH - kGap * (static_cast<float>(kRows) - 1.f)) / static_cast<float>(kRows));
    return {cellW, cellH};
  }

  void render(Canvas& canvas, Rect cell) const {
    float const pad = 8.f;
    float const innerX = cell.x + pad;
    float const innerY = cell.y + pad;
    float const innerW = std::max(cell.width - pad * 2.f, 4.f);
    float const innerH = std::max(cell.height - pad * 2.f, 4.f);
    Rect const inner = Rect::sharp(innerX, innerY, innerW, innerH);

    canvas.setBlendMode(BlendMode::Normal);
    canvas.drawRect(inner, {}, FillStyle::solid(Color::rgb(210, 210, 218)), StrokeStyle::none());

    float const r = std::min(innerW, innerH) * 0.22f;
    Point const c1{inner.x + innerW * 0.34f, inner.y + innerH * 0.5f};
    Point const c2{inner.x + innerW * 0.66f, inner.y + innerH * 0.5f};

    canvas.drawCircle(c1, r, FillStyle::solid(Color{0.05f, 0.75f, 0.85f, 0.62f}), StrokeStyle::none());

    canvas.setBlendMode(mode);
    canvas.drawCircle(c2, r, FillStyle::solid(Color{0.92f, 0.15f, 0.55f, 0.62f}), StrokeStyle::none());

    canvas.setBlendMode(BlendMode::Normal);
    canvas.drawRect(cell, CornerRadius(4.f, 4.f, 4.f, 4.f), FillStyle::none(),
                    StrokeStyle::solid(Color::rgb(90, 90, 100), 1.2f));

    Font const labelFont{.family = ".AppleSystemUIFont", .size = 16.f, .weight = 500.f};
    auto labelLayout =
        Application::instance().textSystem().layout(title, labelFont, Color::rgb(38, 38, 45), 0.f);
    Size const m = labelLayout->measuredSize;
    float const y = cell.y + cell.height - 16.f - m.height;
    float const x = cell.x + (cell.width - m.width) * 0.5f;
    canvas.drawTextLayout(*labelLayout, Point{x, y});
  }
};

struct BlendDemoView {
  /// Passed into each `BlendCell` so `measure` can infer cell size when the row has infinite width.
  Size layoutSize{BlendCell::kDefaultWinW, BlendCell::kDefaultWinH};

  auto body() const {
    auto row = [this](int r) {
      std::vector<Element> cells;
      cells.reserve(static_cast<std::size_t>(BlendCell::kCols));
      for (int col = 0; col < BlendCell::kCols; ++col) {
        DemoCell const& d = kGrid[static_cast<std::size_t>(r * BlendCell::kCols + col)];
        cells.push_back(BlendCell{.mode = d.mode, .title = d.title, .layoutHint = layoutSize});
      }
      return HStack{
          .spacing = BlendCell::kGap,
          .vAlign = VerticalAlignment::Center,
          .children = std::move(cells),
      };
    };

    return VStack {
      .spacing = BlendCell::kGap,
      .padding = BlendCell::kMargin,
      .hAlign = HorizontalAlignment::Center,
      .children =
          {
              row(0),
              row(1),
              row(2),
          },
    };
  }
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  Size const layoutSize{BlendCell::kDefaultWinW, BlendCell::kDefaultWinH};
  app.createWindow<Window>({
      .size = layoutSize,
      .title = "Flux — blend modes",
  }).setView(BlendDemoView{.layoutSize = layoutSize});

  return app.exec();
}
