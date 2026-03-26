#pragma once

#include <memory>
#include <string>

#include <Flux/Core/Types.hpp>

namespace flux {

struct WindowConfig {
  Size size = {1280, 720};
  std::string title = "Flux Application";
  bool fullscreen = false;
  bool resizable = true;
};

class Window {
public:
  ~Window();

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
  Window(Window&&) = delete;
  Window& operator=(Window&&) = delete;

  Size getSize() const;
  void setTitle(std::string title);
  void setFullscreen(bool fullscreen);
  unsigned int handle() const;

  explicit Window(const WindowConfig& config);

private:
  struct Impl;
  std::unique_ptr<Impl> d;
};

} // namespace flux
