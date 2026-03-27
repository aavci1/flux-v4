#include <Flux.hpp>
#include <Flux/Graphics/AttributedString.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextSystem.hpp>

#include <algorithm>
#include <string>

using namespace flux;

namespace {

TextAttribute baseUi() {
  TextAttribute a{};
  a.fontFamily = ".AppleSystemUIFont";
  a.fontSize = 16.f;
  a.fontWeight = 450.f;
  a.color = Color::rgb(28, 28, 34);
  return a;
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
      TextAttribute t = baseUi();
      t.fontSize = 34.f;
      t.fontWeight = 600.f;
      t.color = Color::rgb(18, 18, 24);
      auto layout = ts.layout("Text in Flux", t, 0.f);
      c.drawTextLayout(*layout, Point{margin, y});
      y += layout->measuredSize.height + 8.f;
    }

    // --- Subtitle ---
    {
      TextAttribute t = baseUi();
      t.fontSize = 14.f;
      t.fontWeight = 400.f;
      t.color = Color::rgb(110, 110, 125);
      auto layout = ts.layout("layout · measure · wrap · attributed runs", t, 0.f);
      c.drawTextLayout(*layout, Point{margin, y});
      y += layout->measuredSize.height + 28.f;
    }

    // --- Section: wrapped paragraph ---
    {
      TextAttribute head = baseUi();
      head.fontSize = 13.f;
      head.fontWeight = 600.f;
      head.color = Color::rgb(75, 75, 88);
      auto headLayout = ts.layout("Line wrapping", head, 0.f);
      c.drawTextLayout(*headLayout, Point{margin, y});
      y += headLayout->measuredSize.height + 10.f;

      TextAttribute body = baseUi();
      body.fontSize = 16.f;
      body.fontWeight = 420.f;
      body.color = Color::rgb(38, 38, 48);
      char const* para =
          "TextLayout uses the same Core Text framesetter constraints as measure, so box sizing and "
          "rendered glyphs stay in sync when maxWidth is set. Resize the window to see reflow.";
      auto bodyLayout = ts.layout(para, body, wrapW - 20.0f);
      // Outline uses the same measured size as the laid-out text (single source of truth).
      Size const ms = bodyLayout->measuredSize;
      Rect const box{margin, y, ms.width + 20.0f, ms.height + 20.0f};
      c.drawRect(box, CornerRadius(6.f, 6.f, 6.f, 6.f), FillStyle::none(),
                 StrokeStyle::solid(Color::rgb(200, 200, 210), 1.f));
      c.drawTextLayout(*bodyLayout, Point{margin + 10.0f, y + 10.0f});
      y += bodyLayout->measuredSize.height + 28.f;
    }

    // --- Section: rich text (AttributedString) ---
    {
      TextAttribute head = baseUi();
      head.fontSize = 13.f;
      head.fontWeight = 600.f;
      head.color = Color::rgb(75, 75, 88);
      auto headLayout = ts.layout("Attributed string", head, 0.f);
      c.drawTextLayout(*headLayout, Point{margin, y});
      y += headLayout->measuredSize.height + 10.f;

      // "Swift UIKit AppKit" — three runs, contiguous UTF-8 ranges.
      AttributedString rich{};
      rich.utf8 = "Swift UIKit AppKit";
      TextAttribute a0 = baseUi();
      a0.fontSize = 22.f;
      a0.fontWeight = 520.f;
      a0.color = Colors::blue;

      TextAttribute a1 = baseUi();
      a1.fontSize = 22.f;
      a1.fontWeight = 700.f;
      a1.color = Color::rgb(180, 60, 50);

      TextAttribute a2 = baseUi();
      a2.fontSize = 22.f;
      a2.fontWeight = 520.f;
      a2.color = Color::rgb(40, 140, 75);

      // UTF-8 byte ranges: "Swift "(6) + "UIKit "(6) + "AppKit"(6) = 18 bytes.
      rich.runs = {
          {0, 6, a0},
          {6, 12, a1},
          {12, 18, a2},
      };

      auto richLayout = ts.layout(rich, 0.f);
      c.drawTextLayout(*richLayout, Point{margin, y});
      y += richLayout->measuredSize.height + 28.f;
    }

    // --- Footer: sizes & baselines (informational) ---
    if (y < h - margin) {
      TextAttribute foot = baseUi();
      foot.fontSize = 12.f;
      foot.fontWeight = 400.f;
      foot.color = Color::rgb(140, 140, 155);
      std::string const line =
          std::string("firstBaseline → lastBaseline: layout metrics for alignment APIs.");
      auto footLayout = ts.layout(line, foot, wrapW);
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
