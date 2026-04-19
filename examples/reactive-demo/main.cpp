#include <Flux.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Reactive/Observer.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/Theme.hpp>

#include <algorithm>
#include <cmath>
#include <string>

using namespace flux;

namespace {

Color const kColdFill = Color::rgb(64, 110, 200);
Color const kWarmFill = Color::rgb(220, 96, 140);

/// Holds reactive fields (stable addresses; non-movable Signal / Computed / Animation).
struct DemoState {
  Signal<int> clicks{0};
  Animation<Color> fillColor{kColdFill};
  Animation<Point> circleCenter{Point{360.f, 260.f}};
  Signal<Point> pointerPos{Point{0.f, 0.f}};

  Computed<float> distanceToPointer{[&]() {
    Point const c = circleCenter.get();
    Point const p = pointerPos.get();
    float const dx = c.x - p.x;
    float const dy = c.y - p.y;
    return std::sqrt(dx * dx + dy * dy);
  }};
};

class ReactiveDemoWindow : public Window {
  DemoState s_;
  ObserverHandle hClicks_{};
  ObserverHandle hFill_{};
  ObserverHandle hCircle_{};
  ObserverHandle hPointer_{};
  ObserverHandle hDistance_{};
  /// Previous window content size; used to scale pointer/circle when the window is resized.
  Size lastLayout_{};

public:
  explicit ReactiveDemoWindow(WindowConfig const& c) : Window(c), lastLayout_{c.size} {
    auto redraw = [this]() { Window::requestRedraw(); };
    hClicks_ = s_.clicks.observe(redraw);
    hFill_ = s_.fillColor.observe(redraw);
    hCircle_ = s_.circleCenter.observe(redraw);
    hPointer_ = s_.pointerPos.observe(redraw);
    hDistance_ = s_.distanceToPointer.observe(redraw);

    Application::instance().eventQueue().on<WindowEvent>([this](WindowEvent const& ev) {
      if (ev.handle != handle() || ev.kind != WindowEvent::Kind::Resize) {
        return;
      }
      Size const newS = ev.size;
      if (newS.width < 1.f || newS.height < 1.f) {
        return;
      }
      if (lastLayout_.width >= 1.f && lastLayout_.height >= 1.f) {
        float const sx = newS.width / lastLayout_.width;
        float const sy = newS.height / lastLayout_.height;
        Point const p = s_.pointerPos.get();
        s_.pointerPos.set(Point{p.x * sx, p.y * sy});
        Point const c = s_.circleCenter.get();
        s_.circleCenter.set(Point{c.x * sx, c.y * sy}, Transition::instant());
      }
      lastLayout_ = newS;
    });

    Application::instance().eventQueue().on<InputEvent>([this](InputEvent const& e) {
      if (e.handle != Window::handle()) {
        return;
      }
      if (e.kind == InputEvent::Kind::PointerMove) {
        s_.pointerPos.set(Point{e.position.x, e.position.y});
        return;
      }
      if (e.kind != InputEvent::Kind::PointerDown) {
        return;
      }

      s_.clicks.set(s_.clicks.get() + 1);

      {
        WithTransition spring{Transition::spring(500.f, 25.f, 0.55f)};
        Color const target = (s_.clicks.get() % 2 == 0) ? kColdFill : kWarmFill;
        s_.fillColor.set(target);
      }

      {
        WithTransition ease{Transition::ease(0.4f)};
        Point const p{e.position.x, e.position.y};
        s_.circleCenter.set(p);
      }
    });
  }

  ~ReactiveDemoWindow() {
    s_.clicks.unobserve(hClicks_);
    s_.fillColor.unobserve(hFill_);
    s_.circleCenter.unobserve(hCircle_);
    s_.pointerPos.unobserve(hPointer_);
    s_.distanceToPointer.unobserve(hDistance_);
  }

  void render(Canvas& c) override {
    TextSystem& ts = Application::instance().textSystem();

    Rect const vb = c.clipBounds();
    Size const sz = Window::getSize();
    float const w = std::max({vb.width, sz.width, 1.f});
    float const h = std::max({vb.height, sz.height, 1.f});

    Color const bg = Color::rgb(245, 246, 250);
    c.clear(bg);

    const float margin = 24.f;
    Rect const panel =
        Rect::sharp(margin, margin, std::max(w - margin * 2.f, 40.f), std::max(h - margin * 2.f, 40.f));

    Color const cardColor = s_.fillColor.get();
    c.drawRect(panel, CornerRadius(18.f, 18.f, 18.f, 18.f), FillStyle::solid(cardColor),
               StrokeStyle::solid(Color::rgb(255, 255, 255), 1.5f));

    Point const center = s_.circleCenter.get();
    const float r = 36.f;
    c.drawCircle(center, r, FillStyle::solid(Color::rgb(255, 255, 255)), StrokeStyle::solid(Color::rgb(40, 44, 55), 2.f));

    Theme const theme = Theme::light();
    Font const bodyFont = theme.fontBody;
    Font const hintFont = theme.fontBodySmall;

    std::string const line = std::string("Signal<int> clicks: ") + std::to_string(s_.clicks.get()) +
                             "  |  Computed<float> dist: " +
                             std::to_string(static_cast<int>(s_.distanceToPointer.get())) +
                             " px  |  Animation<Color> (spring) + Animation<Point> (ease)";
    auto bodyLayout = ts.layout(line, bodyFont, theme.colorTextPrimary, panel.width - 32.f);
    c.drawTextLayout(*bodyLayout, Point{panel.x + 16.f, panel.y + 16.f});

    auto hintLayout = ts.layout("Click: spring palette + ease circle to click. Move pointer: Computed distance updates.",
                                hintFont, theme.colorTextSecondary, panel.width - 32.f);
    c.drawTextLayout(*hintLayout, Point{panel.x + 16.f, panel.y + panel.height - 52.f});
  }
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  app.createWindow<ReactiveDemoWindow>({
      .size = {800, 800},
      .title = "Flux — Reactive & animation demo",
      .resizable = true,
  });

  return app.exec();
}
