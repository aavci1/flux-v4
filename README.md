# Flux v4

A small **C++23** application framework for **macOS** with a **Metal** 2D canvas, vector paths (tessellated via [libtess2](https://github.com/memononen/libtess2)), and **CoreText**-backed text layout and glyph rasterization.

Linux desktop (Wayland/Vulkan) and embedded Linux (KMS/DRM) are reserved in CMake but not implemented yet.

## Build

Requirements: **CMake 3.25+**, **Xcode command-line tools** (Metal compiler `xcrun metal`, `xxd` for embedded shader bytecode).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Executables (all link the static `flux` library):

| Target | Description |
|--------|-------------|
| `hello_world` | Minimal window and `Application::exec()` |
| `clock_demo` | Timers and drawing |
| `blend_demo` | Opacity and blend modes |
| `text_demo` | `TextSystem`, `AttributedString`, `Canvas::drawTextLayout` |

Configure-time options:

- `FLUX_PLATFORM` — `AUTO` (default), `MACOS`, or reserved future values (`LINUX_WAYLAND`, `LINUX_KMS`).
- `FLUX_ENABLE_DEFAULT_EVENT_LOGGING` — default **ON**; set **OFF** to silence stdout logging from built-in `Application` handlers.

## Documentation

- [Documentation index](docs/README.md) — table of contents for `docs/`.
- [Architecture](docs/architecture.md) — layers, rendering loop, graphics stack.
- [Conventions](docs/conventions.md) — layout, naming, pimpl, platform boundaries.
- [Event queue](docs/event_queue.md) — `EventQueue`, dispatch order, timers, custom events.

Public API headers live under `include/Flux/`. The umbrella header is [`include/Flux.hpp`](include/Flux.hpp).
