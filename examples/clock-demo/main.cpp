#include <Flux.hpp>

#include <chrono>
#include <ctime>
#include <memory>

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

  app.eventQueue().on<TimerEvent>([&window, windowHandle](TimerEvent const& e) {
    if (e.windowHandle != 0 && e.windowHandle != windowHandle) {
      return;
    }
    window.syncTimeFromSystem();
    flux::Window::postRedraw(windowHandle);
  });
  app.scheduleRepeatingTimer(std::chrono::seconds(1), windowHandle);

  return app.exec();
}
