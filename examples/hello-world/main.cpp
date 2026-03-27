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
    TextSystem& ts = Application::instance().textSystem();

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

    auto textLayout = ts.layout("Hello, World!", labelAttr, vb, {
      .horizontalAlignment = HorizontalAlignment::Center,
      .verticalAlignment = VerticalAlignment::Center,
      .wrapping = TextWrapping::WrapAnywhere,
    });
    c.drawTextLayout(*textLayout, Point{vb.x, vb.y});
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
