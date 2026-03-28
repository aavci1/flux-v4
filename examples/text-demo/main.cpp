#include <Flux.hpp>
#include <Flux/Graphics/AttributedString.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextSystem.hpp>

#include <algorithm>
#include <string>

using namespace flux;

namespace {

Font baseFont() {
  Font f{};
  f.family = ".AppleSystemUIFont";
  f.size = 16.f;
  f.weight = 450.f;
  return f;
}

class TextDemoWindow : public Window {
public:
  explicit TextDemoWindow(WindowConfig const& c) : Window(c) {}

  void render(Canvas& c) override {
    c.clear(Color::rgb(250, 250, 252));

    Rect const vb = c.clipBounds();
    Size const sz = getSize();
    float const w = std::max({vb.width, sz.width, 1.f});
    float const h = std::max({vb.height, sz.height, 1.f});

    float const margin = 40.f;
    float const contentW = std::max(w - margin * 2.f, 120.f);
    float const wrapW = contentW; // std::min(520.f, contentW);

    TextSystem& ts = Application::instance().textSystem();
    float y = margin;

    // --- Title ---
    {
      Font t = baseFont();
      t.size = 34.f;
      t.weight = 600.f;
      auto layout = ts.layout("Text in Flux", t, Color::rgb(18, 18, 24), 0.f);
      c.drawTextLayout(*layout, Point{margin, y});
      y += layout->measuredSize.height + 8.f;
    }

    // --- Subtitle ---
    {
      Font t = baseFont();
      t.size = 14.f;
      t.weight = 400.f;
      auto layout = ts.layout("layout · measure · wrap · attributed runs", t, Color::rgb(110, 110, 125), 0.f);
      c.drawTextLayout(*layout, Point{margin, y});
      y += layout->measuredSize.height + 28.f;
    }

    // --- Section: wrapped paragraph ---
    {
      Font head = baseFont();
      head.size = 13.f;
      head.weight = 600.f;
      auto headLayout = ts.layout("Line wrapping", head, Color::rgb(75, 75, 88), 0.f);
      c.drawTextLayout(*headLayout, Point{margin, y});
      y += headLayout->measuredSize.height + 10.f;

      char const* para = "TextLayout uses the same Core Text framesetter constraints as measure, so box sizing and rendered glyphs stay in sync when maxWidth is set. Resize the window to see reflow.";
      Font body = baseFont();
      body.size = 16.f;
      body.weight = 420.f;

      auto bodyLayout = ts.layout(para, body, Color::rgb(38, 38, 48), Rect{0.0f, 0, contentW - 20.0f, 200.0f}, {
        .horizontalAlignment = HorizontalAlignment::Center,
        .verticalAlignment = VerticalAlignment::Top
      });

      // Outline uses the same measured size as the laid-out text (single source of truth).
      Size const ms = bodyLayout->measuredSize;
      Rect const box{margin, y, contentW, 200.0f};
      c.drawRect(box, CornerRadius(6.f, 6.f, 6.f, 6.f), FillStyle::none(),
                 StrokeStyle::solid(Color::rgb(200, 200, 210), 1.f));
      c.drawTextLayout(*bodyLayout, Point{margin + 10.0f, y + 10.0f});
      y += 200.0f + 28.f;
    }

    // --- Section: rich text (AttributedString) ---
    {
      Font head = baseFont();
      head.size = 13.f;
      head.weight = 600.f;
      auto headLayout = ts.layout("Attributed string", head, Color::rgb(75, 75, 88), 0.f);
      c.drawTextLayout(*headLayout, Point{margin, y});
      y += headLayout->measuredSize.height + 10.f;

      // "Swift UIKit AppKit" — three runs, contiguous UTF-8 ranges.
      AttributedString rich{};
      rich.utf8 = "Swift UIKit AppKit";
      Font a0 = baseFont();
      a0.size = 22.f;
      a0.weight = 520.f;

      Font a1 = baseFont();
      a1.size = 22.f;
      a1.weight = 700.f;

      Font a2 = baseFont();
      a2.size = 22.f;
      a2.weight = 520.f;

      // UTF-8 byte ranges: "Swift "(6) + "UIKit "(6) + "AppKit"(6) = 18 bytes.
      rich.runs = {
          {0, 6, a0, Colors::blue},
          {6, 12, a1, Color::rgb(180, 60, 50)},
          {12, 18, a2, Color::rgb(40, 140, 75)},
      };

      auto richLayout = ts.layout(rich, 0.f);
      c.drawTextLayout(*richLayout, Point{margin, y});
      y += richLayout->measuredSize.height + 28.f;
    }

    // --- Footer: sizes & baselines (informational) ---
    if (y < h - margin) {
      Font foot = baseFont();
      foot.size = 12.f;
      foot.weight = 400.f;
      std::string const line =
          std::string("firstBaseline → lastBaseline: layout metrics for alignment APIs.");
      auto footLayout = ts.layout(line, foot, Color::rgb(140, 140, 155), wrapW);
      float const fy = std::min(y, h - margin - footLayout->measuredSize.height);
      c.drawTextLayout(*footLayout, Point{margin, fy});
    }
  }
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  app.createWindow<TextDemoWindow>({
      .size = {720, 640},
      .title = "Flux — Text demo",
  });

  return app.exec();
}
