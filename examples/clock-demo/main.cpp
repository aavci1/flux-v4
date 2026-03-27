#include <Flux.hpp>

#include <chrono>
#include <ctime>
#include <memory>

#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#endif

#include "clock.hpp"

namespace {

class ClockWindow : public flux::Window {
  int hours_ = 0;
  int minutes_ = 0;
  int seconds_ = 0;

public:
  explicit ClockWindow(flux::WindowConfig const& c) : Window(c) {}

  void render(flux::Canvas& c) override {
    const auto sz = getSize();
    flux::Rect bounds = flux::Rect::sharp(0.f, 0.f, static_cast<float>(sz.width), static_cast<float>(sz.height));
    c.clear(flux::Color::rgb(245, 245, 248));
    clock_demo::drawClock(c, bounds, hours_, minutes_, seconds_);
  }

  void syncTimeFromSystem() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* local = std::localtime(&t);
    if (!local) {
      return;
    }
    hours_ = local->tm_hour;
    minutes_ = local->tm_min;
    seconds_ = local->tm_sec;
  }
};

} // namespace

int main(int argc, char* argv[]) {
  flux::Application app(argc, argv);

  auto& window = app.createWindow<ClockWindow>({
      .size = {800, 800},
      .title = "Clock",
  });
  const unsigned int windowHandle = window.handle();

  window.syncTimeFromSystem();

#if defined(__APPLE__)
  struct DispatchSourceDeleter {
    void operator()(dispatch_source_t s) const {
      if (s) {
        dispatch_source_cancel(s);
        dispatch_release(s);
      }
    }
  };
  using DispatchSource = std::unique_ptr<std::remove_pointer_t<dispatch_source_t>, DispatchSourceDeleter>;
  DispatchSource tick{dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue()),
                      DispatchSourceDeleter{}};
  if (tick) {
    dispatch_source_set_timer(tick.get(), dispatch_time(DISPATCH_TIME_NOW, 0), NSEC_PER_SEC, NSEC_PER_SEC / 10);
    dispatch_source_set_event_handler(tick.get(), ^{
      window.syncTimeFromSystem();
      flux::Window::postRedraw(windowHandle);
    });
    dispatch_resume(tick.get());
  }
#endif

  return app.exec();
}
