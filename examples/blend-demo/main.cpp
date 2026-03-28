#include <Flux.hpp>
#include <Flux/Core/Application.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextSystem.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

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

constexpr int kCols = 4;
constexpr int kRows = 3;

void drawCell(Canvas& c, Rect cell, BlendMode mode, const char* title,
              std::chrono::steady_clock::time_point start) {
  using clock = std::chrono::steady_clock;
  const float t = std::chrono::duration<float>(clock::now() - start).count();
  const float pulse = 0.92f + 0.08f * std::sin(t * 2.5f);

  const float pad = 8.f;
  const float innerX = cell.x + pad;
  const float innerY = cell.y + pad;
  const float innerW = std::max(cell.width - pad * 2.f, 4.f);
  const float innerH = std::max(cell.height - pad * 2.f, 4.f);
  Rect inner = Rect::sharp(innerX, innerY, innerW, innerH);

  c.setBlendMode(BlendMode::Normal);
  c.drawRect(inner, {}, FillStyle::solid(Color::rgb(210, 210, 218)), StrokeStyle::none());

  const float r = std::min(innerW, innerH) * 0.22f * pulse;
  const Point c1{inner.x + innerW * 0.34f, inner.y + innerH * 0.5f};
  const Point c2{inner.x + innerW * 0.66f, inner.y + innerH * 0.5f};

  c.drawCircle(c1, r, FillStyle::solid(Color{0.05f, 0.75f, 0.85f, 0.62f}), StrokeStyle::none());

  c.setBlendMode(mode);
  c.drawCircle(c2, r, FillStyle::solid(Color{0.92f, 0.15f, 0.55f, 0.62f}), StrokeStyle::none());

  c.setBlendMode(BlendMode::Normal);
  c.drawRect(cell, CornerRadius(4.f, 4.f, 4.f, 4.f), FillStyle::none(),
               StrokeStyle::solid(Color::rgb(90, 90, 100), 1.2f));

  Font labelFont{.family = ".AppleSystemUIFont", .size = 16.f, .weight = 500.f};

  auto labelLayout = Application::instance().textSystem().layout(title, labelFont, Color::rgb(38, 38, 45), 0.f);
  Size const m = labelLayout->measuredSize;
  float const y = cell.y + cell.height - 16.f - m.height;
  float const x = cell.x + (cell.width - m.width) * 0.5f;
  c.drawTextLayout(*labelLayout, Point{x, y});
}

class BlendDemoWindow : public Window {
  std::chrono::steady_clock::time_point start_{};

public:
  explicit BlendDemoWindow(WindowConfig const& c) : Window(c), start_(std::chrono::steady_clock::now()) {}

  void render(Canvas& c) override {
    c.clear(Color{0.96f, 0.96f, 0.98f, 1.f});

    const Rect vb = c.clipBounds();
    const Size sz = getSize();
    const float w = std::max({vb.width, sz.width, 1.f});
    const float h = std::max({vb.height, sz.height, 1.f});

    const float margin = 24.f;
    const float gw = w - margin * 2.f;
    const float gh = h - margin * 2.f;
    const float cw = gw / static_cast<float>(kCols);
    const float ch = gh / static_cast<float>(kRows);
    const float gap = 10.f;

    for (size_t i = 0; i < kGrid.size(); ++i) {
      const int col = static_cast<int>(i % static_cast<size_t>(kCols));
      const int row = static_cast<int>(i / static_cast<size_t>(kCols));
      Rect cell = Rect::sharp(margin + static_cast<float>(col) * cw + gap * 0.5f,
                                          margin + static_cast<float>(row) * ch + gap * 0.5f,
                                          cw - gap, ch - gap);
      drawCell(c, cell, kGrid[i].mode, kGrid[i].title, start_);
    }
  }
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  app.createWindow<BlendDemoWindow>({
      .size = {920, 700},
      .title = "Flux — blend modes",
  });

  return app.exec();
}
