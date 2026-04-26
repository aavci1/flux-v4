#include <Flux/Core/Application.hpp>

#include <cstdlib>
#include <stdexcept>

namespace flux {

struct Application::Impl {};

Application::Application(int, char**) {}
Application::~Application() = default;

int Application::exec() { return 0; }
void Application::quit() {}
void Application::requestRedraw() {}
void Application::requestWindowRedraw(unsigned int) {}
void Application::flushRedraw() {}

std::uint64_t Application::scheduleRepeatingTimer(std::chrono::nanoseconds, unsigned int) {
  return 0;
}

void Application::cancelTimer(std::uint64_t) {}

bool Application::hasInstance() { return false; }

Application& Application::instance() {
  throw std::runtime_error("Application runtime is rebuilt in v5 Stage 4");
}

EventQueue& Application::eventQueue() { std::abort(); }
TextSystem& Application::textSystem() { std::abort(); }
Clipboard& Application::clipboard() { std::abort(); }
void* Application::iconFontHandle() const { return nullptr; }

ObserverHandle Application::onNextFrameNeeded(std::function<void()>) { return {}; }
void Application::unobserveNextFrame(ObserverHandle) {}

bool Application::isMainThread() const noexcept { return true; }
void Application::wakeEventLoop() {}
void Application::requestAnimationFrames() {}
void Application::adoptOwnedWindow(std::unique_ptr<Window>) {}
void Application::onWindowRegistered(Window*) {}
void Application::unregisterWindowHandle(unsigned int) {}
void Application::processFrameCallbacks() {}
void Application::presentRequestedWindows(bool, bool) {}

} // namespace flux
