#include <Flux.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <chrono>
#include <cmath>
#include <ctime>

#include "clock.hpp"

using namespace flux;

struct ClockFace : ViewModifiers<ClockFace> {
  Size measure(LayoutConstraints const& c, LayoutHints const&) const {
    float const s = std::min(
        std::isfinite(c.maxWidth) ? c.maxWidth : 360.f,
        std::isfinite(c.maxHeight) ? c.maxHeight : 360.f);
    return {s, s};
  }

  /// Angles come from wall-clock time at draw time — independent of layout/rebuild cadence.
  void render(Canvas& canvas, Rect frame) const {
    using namespace std::chrono;
    auto const now = system_clock::now();
    std::time_t const tt = system_clock::to_time_t(now);
    double const fracSec = duration<double>(now - system_clock::from_time_t(tt)).count();
    std::tm* local = std::localtime(&tt);
    if (!local) {
      return;
    }
    int const h12 = local->tm_hour % 12;
    float const s = static_cast<float>(local->tm_sec) + static_cast<float>(fracSec);
    float const hDeg = static_cast<float>(h12) * 30.f + static_cast<float>(local->tm_min) * 0.5f + s / 120.f;
    float const mDeg = static_cast<float>(local->tm_min) * 6.f + s * 0.1f;
    float const sDeg = s * 6.f;
    clock_demo::drawClock(canvas, frame, hDeg, mDeg, sDeg);
  }
};

struct ClockView {
  auto body() const {
    return ZStack{.children = children(Rectangle{}.fill(FillStyle::solid(Color::hex(0xF5F5F8))), ClockFace{})};
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {800, 800},
      .title = "Flux — Clock",
      .resizable = true,
  });
  w.setView<ClockView>();

  // Redraw ~60 Hz so the hands track real time (scene rebuild is not required; `render()` reads `now()`).
  unsigned int const windowHandle = w.handle();
  std::uint64_t const redrawTimerId =
      app.scheduleRepeatingTimer(std::chrono::nanoseconds(1'000'000'000 / 60), windowHandle);
  app.eventQueue().on<TimerEvent>([&w, windowHandle, redrawTimerId](TimerEvent const& e) {
    if (e.timerId != redrawTimerId) {
      return;
    }
    if (e.windowHandle != 0 && e.windowHandle != windowHandle) {
      return;
    }
    w.requestRedraw();
  });

  return app.exec();
}
