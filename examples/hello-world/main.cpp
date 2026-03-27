#include <Flux.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Styles.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>

#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#endif

using namespace flux;

namespace {

class HelloWindow : public Window {
  std::chrono::steady_clock::time_point start_{};

public:
  explicit HelloWindow(WindowConfig const& c) : Window(c), start_(std::chrono::steady_clock::now()) {}

  void render(Canvas& c) override {
    using clock = std::chrono::steady_clock;
    const float t = std::chrono::duration<float>(clock::now() - start_).count();

    c.clear(Color{1.0f, 1.0f, 1.0f, 1.f});

    // `clipBounds()` comes from the canvas viewport (updated in `beginFrame`) and stays valid before
    // `NSView` has reported a non-zero size — `getSize()` alone can be 0×0 on early frames.
    const Rect vb = c.clipBounds();
    const Size sz = getSize();
    const float w = std::max({vb.width, sz.width, 1.f});
    const float h = std::max({vb.height, sz.height, 1.f});
    const float cx = w * 0.5f;
    const float cy = h * 0.5f;
    const float pulse = 0.5f + 0.5f * std::sin(t * 4.f);
    const float r = 48.f + 24.f * pulse;

    c.save();
    c.translate(Point{cx, cy});
    c.rotate(t * 0.5f);
    Rect card = Rect::sharp(-r, -r, r * 2.f, r * 2.f);
    c.drawRect(card, CornerRadius(16.f), FillStyle::solid(Color::rgb(230, 240, 255)),
               StrokeStyle::solid(Color::rgb(33, 150, 243), 3.f));
    c.restore();
  }
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& window = app.createWindow<HelloWindow>({
      .size = {400, 400},
      .title = "Flux Canvas",
  });
  const unsigned int windowHandle = window.handle();

#if defined(__APPLE__)
  // Drive redraws from the main queue so posts line up with the run loop (vs. a background thread
  // racing `sleep_until`, which fights coalesced redraws and display pacing).
  struct DispatchSourceDeleter {
    void operator()(dispatch_source_t s) const {
      if (s) {
        dispatch_source_cancel(s);
        dispatch_release(s);
      }
    }
  };
  using DispatchSource = std::unique_ptr<std::remove_pointer_t<dispatch_source_t>, DispatchSourceDeleter>;
  DispatchSource redrawTimer{
      dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue()),
      DispatchSourceDeleter{}};
  if (redrawTimer) {
    dispatch_source_set_timer(redrawTimer.get(), dispatch_time(DISPATCH_TIME_NOW, 0), NSEC_PER_SEC / 60,
                              NSEC_PER_SEC / 360);
    dispatch_source_set_event_handler(redrawTimer.get(), ^{
      Window::postRedraw(windowHandle);
    });
    dispatch_resume(redrawTimer.get());
  }
#endif

  return app.exec();
}
