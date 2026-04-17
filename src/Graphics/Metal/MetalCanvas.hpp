#pragma once

#include <Flux/Graphics/Canvas.hpp>

#include <memory>

namespace flux {

class TextSystem;
class Window;

/// Creates the Metal-backed canvas for a window (macOS only).
std::unique_ptr<Canvas> createMetalCanvas(Window* window, void* caMetalLayer, unsigned int handle,
                                          TextSystem& textSystem);

/// When `sync` is true, the next `present()` uses commit → waitUntilScheduled → drawable present (resize-safe).
/// No-op if `canvas` is not a Metal-backed canvas.
void setSyncPresentForCanvas(Canvas* canvas, bool sync);

/// Waits for the most recently submitted Metal frame for this canvas to complete on the GPU.
/// No-op if `canvas` is not a Metal-backed canvas or no frame has been submitted yet.
void waitForCanvasLastPresentComplete(Canvas* canvas);

} // namespace flux
