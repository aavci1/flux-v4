#include <Flux.hpp>
#include <Flux/Graphics/AttributedString.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Graphics/Styles.hpp>

#include <algorithm>

using namespace flux;

namespace {

class HelloWindow : public Window {
public:
  explicit HelloWindow(WindowConfig const& c) : Window(c) {}

  void render(Canvas& c) override {
    c.clear(Color{1.0f, 1.0f, 1.0f, 1.f});

    // `clipBounds()` matches the canvas viewport (valid before `NSView` reports a non-zero size).
    const Rect vb = c.clipBounds();
    const Size sz = getSize();
    const float w = std::max({vb.width, sz.width, 1.f});
    const float h = std::max({vb.height, sz.height, 1.f});

    TextAttribute labelAttr{};
    labelAttr.fontFamily = ".AppleSystemUIFont";
    labelAttr.fontSize = 28.f;
    labelAttr.fontWeight = 500.f;
    labelAttr.color = Colors::darkGray;

    auto textLayout = Application::instance().textSystem().shapePlain("Hello, World!", labelAttr, 0.f);
    Size const m = textLayout->measuredSize;
    // Center the first baseline in the viewport (avoids the line-box center looking optically low).
    float const y = h * 0.5f - textLayout->firstBaseline;
    float const x = (w - m.width) * 0.5f;

    c.drawTextLayout(*textLayout, Point{x, y});
  }
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  app.createWindow<HelloWindow>({
      .size = {400, 400},
      .title = "Hello, World!",
  });

  return app.exec();
}
