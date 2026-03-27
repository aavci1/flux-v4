#include <Flux.hpp>
#include <Flux/Core/Application.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Styles.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>

#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#endif

namespace {

struct DemoCell {
  flux::BlendMode mode;
  const char* title;
};

// Row-major grid: each cell shows cyan + magenta circles; second circle uses `mode`.
constexpr std::array<DemoCell, 12> kGrid{{
    {flux::BlendMode::Normal, "Normal"},
    {flux::BlendMode::Multiply, "Multiply"},
    {flux::BlendMode::Screen, "Screen"},
    {flux::BlendMode::Darken, "Darken"},
    {flux::BlendMode::Lighten, "Lighten"},
    {flux::BlendMode::DstOver, "DstOver"},
    {flux::BlendMode::SrcIn, "SrcIn"},
    {flux::BlendMode::DstIn, "DstIn"},
    {flux::BlendMode::SrcOut, "SrcOut"},
    {flux::BlendMode::DstOut, "DstOut"},
    {flux::BlendMode::Src, "Src"},
    {flux::BlendMode::Dst, "Dst"},
}};

constexpr int kCols = 4;
constexpr int kRows = 3;

void printGridLegend() {
  std::fprintf(stderr, "flux blend_demo — grid (row-major, left-to-right):\n");
  for (int r = 0; r < kRows; ++r) {
    std::fprintf(stderr, "  row %d: ", r);
    for (int c = 0; c < kCols; ++c) {
      const size_t i = static_cast<size_t>(r * kCols + c);
      if (i < kGrid.size()) {
        std::fprintf(stderr, "%s%s", kGrid[i].title, (c < kCols - 1) ? " | " : "");
      }
    }
    std::fprintf(stderr, "\n");
  }
  std::fflush(stderr);
}

void drawCell(flux::Canvas& c, flux::Rect cell, flux::BlendMode mode,
              std::chrono::steady_clock::time_point start) {
  using clock = std::chrono::steady_clock;
  const float t = std::chrono::duration<float>(clock::now() - start).count();
  const float pulse = 0.92f + 0.08f * std::sin(t * 2.5f);

  const float pad = 8.f;
  const float innerX = cell.x + pad;
  const float innerY = cell.y + pad;
  const float innerW = std::max(cell.width - pad * 2.f, 4.f);
  const float innerH = std::max(cell.height - pad * 2.f, 4.f);
  flux::Rect inner = flux::Rect::sharp(innerX, innerY, innerW, innerH);

  c.setBlendMode(flux::BlendMode::Normal);
  c.setStrokeStyle(flux::StrokeStyle::none());
  c.setFillStyle(flux::FillStyle::solid(flux::Color::rgb(210, 210, 218)));
  c.drawRect(inner, {});

  const float r = std::min(innerW, innerH) * 0.22f * pulse;
  const flux::Point c1{inner.x + innerW * 0.34f, inner.y + innerH * 0.5f};
  const flux::Point c2{inner.x + innerW * 0.66f, inner.y + innerH * 0.5f};

  c.setFillStyle(flux::FillStyle::solid(flux::Color{0.05f, 0.75f, 0.85f, 0.62f}));
  c.drawCircle(c1, r);

  c.setBlendMode(mode);
  c.setFillStyle(flux::FillStyle::solid(flux::Color{0.92f, 0.15f, 0.55f, 0.62f}));
  c.drawCircle(c2, r);

  c.setBlendMode(flux::BlendMode::Normal);
  c.setStrokeStyle(flux::StrokeStyle::solid(flux::Color::rgb(90, 90, 100), 1.2f));
  c.setFillStyle(flux::FillStyle::none());
  c.drawRect(cell, flux::CornerRadius(4.f, 4.f, 4.f, 4.f));
}

class BlendDemoWindow : public flux::Window {
  std::chrono::steady_clock::time_point start_{};

public:
  explicit BlendDemoWindow(flux::WindowConfig const& c) : flux::Window(c), start_(std::chrono::steady_clock::now()) {
    printGridLegend();
  }

  void render(flux::Canvas& c) override {
    c.clear(flux::Color{0.96f, 0.96f, 0.98f, 1.f});

    const flux::Rect vb = c.clipBounds();
    const flux::Size sz = getSize();
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
      flux::Rect cell = flux::Rect::sharp(margin + static_cast<float>(col) * cw + gap * 0.5f,
                                          margin + static_cast<float>(row) * ch + gap * 0.5f,
                                          cw - gap, ch - gap);
      drawCell(c, cell, kGrid[i].mode, start_);
    }
  }
};

} // namespace

int main(int argc, char* argv[]) {
  flux::Application app(argc, argv);

  auto& blendWindow = app.createWindow<BlendDemoWindow>({
      .size = {920, 700},
      .title = "Flux — blend modes (see stderr for grid legend)",
  });

  return app.exec();
}
