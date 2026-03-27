#pragma once

#include <memory>
#include <string>

#include <Flux/Core/Types.hpp>

namespace flux {

class Window;
class Canvas;

/// Internal abstract platform window; implemented in platform translation units. Not part of the public API.
class PlatformWindow {
public:
  virtual ~PlatformWindow() = default;

  virtual void setFluxWindow(Window* window) = 0;

  virtual std::unique_ptr<Canvas> createCanvas(Window& owner) = 0;

  virtual void resize(const Size& newSize) = 0;
  virtual void setFullscreen(bool fullscreen) = 0;
  virtual void setTitle(const std::string& title) = 0;

  virtual Size currentSize() const = 0;
  virtual bool isFullscreen() const = 0;
  virtual unsigned int handle() const = 0;

  virtual void* nativeGraphicsSurface() const = 0;
};

} // namespace flux
