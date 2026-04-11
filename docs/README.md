# Flux v4 — documentation

| Document | Contents |
|----------|----------|
| [architecture.md](architecture.md) | Layers (core, UI, scene, reactive, platform, graphics), main loop, Metal canvas, text pipeline, dependencies |
| [conventions.md](conventions.md) | Repository layout, CMake, namespaces, pimpl, naming, includes, examples |
| [layout-system.md](layout-system.md) | Constraints, `LayoutEngine`, build/measure pipeline, flex, **`Element` modifiers** (flat `ElementModifiers`, **`LayoutHints`** forwarding), `ViewModifiers` chaining |
| [ui-view-body-style.md](ui-view-body-style.md) | Indentation and structure for `body()` returning declarative view trees; wrapping and modifier chains |
| [friction-points.md](friction-points.md) | Historical pain points vs current mitigations (e.g. `ContainerBuildScope`, `FLUX_ELEMENT_MODEL`, flat modifiers) |
| [event_queue.md](event_queue.md) | `Event` variants, `EventQueue` API, dispatch order, threading, macOS integration |
| [TextArea-spec.md](TextArea-spec.md) | Multiline text field behavior notes: `LineRange`/`LineMetrics` space, vertical nav, scroll clamp (now implemented by multiline `TextInput`) |

The project root [README.md](../README.md) has build commands and the full list of example executable targets.
