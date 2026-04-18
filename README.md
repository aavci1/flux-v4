# Flux v4

A small **C++23** application framework for **macOS** with a **Metal** 2D canvas, vector paths (tessellated via [libtess2](https://github.com/memononen/libtess2)), and **CoreText**-backed text layout and glyph rasterization. It also provides an optional **declarative UI** layer built on a retained **scene tree**, plus **reactive** primitives (`Signal`, `Computed`, `Animated`, transitions) coordinated with the main loop.

Linux desktop (Wayland/Vulkan) and embedded Linux (KMS/DRM) are reserved in CMake but not implemented yet.

## Build

Requirements: **CMake 3.25+**, **Xcode command-line tools** (Metal compiler `xcrun metal`, `xxd` for embedded shader bytecode).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DFLUX_BUILD_EXAMPLES=ON
cmake --build build
```

By default only the static `flux` library is built. Pass **`-DFLUX_BUILD_EXAMPLES=ON`** (as above) to build the sample executables below (each links `flux`):

| Target | Description |
|--------|-------------|
| `hello_world` | Minimal window and `Application::exec()` |
| `clock_demo` | Timers and canvas drawing |
| `blend_demo` | Opacity and blend modes |
| `text_demo` | `TextSystem`, `AttributedString`, `Canvas::drawTextLayout` |
| `image_demo` | Loading and drawing `Image` from a file |
| `reactive_demo` | `Signal` / `Computed` / `Animated` and observers |
| `card_demo` | Declarative UI (`setView`), stacks, state hooks, animation |

Configure-time options:

- `FLUX_PLATFORM` — `AUTO` (default), `MACOS`, or reserved future values (`LINUX_WAYLAND`, `LINUX_KMS`).
- `FLUX_ENABLE_DEFAULT_EVENT_LOGGING` — default **OFF**; set **ON** to print built-in `Application` event handlers to stdout (useful while debugging).
- `FLUX_BUILD_EXAMPLES` — default **OFF**; set **ON** to build example executables under [`examples/CMakeLists.txt`](examples/CMakeLists.txt).

## Documentation

- [Documentation index](docs/README.md) — table of contents for `docs/`.
- [Conventions](docs/conventions.md) — layout, naming, pimpl, platform boundaries.
- [Event queue](docs/event_queue.md) — `EventQueue`, dispatch order, timers, custom events.

Public API headers live under `include/Flux/`. The umbrella header is [`include/Flux.hpp`](include/Flux.hpp) (core, graphics, and [`Reactive.hpp`](include/Flux/Reactive/Reactive.hpp)). Declarative UI pulls in [`UI/UI.hpp`](include/Flux/UI/UI.hpp) (also includes `WindowUI.hpp` for `Window::setView`).
