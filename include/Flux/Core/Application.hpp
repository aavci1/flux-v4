#pragma once

#include <memory>

#include <Flux/Core/Window.hpp>

namespace flux {

class EventQueue;

class Application {
public:
  explicit Application(int argc = 0, char** argv = nullptr);
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&) = delete;
  Application& operator=(Application&&) = delete;

  Window& createWindow(const WindowConfig& config);

  int exec();
  void quit();

  static Application& instance();

  EventQueue& eventQueue();

private:
  struct Impl;
  std::unique_ptr<Impl> d;
};

} // namespace flux
