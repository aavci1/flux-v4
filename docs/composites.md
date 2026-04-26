# Retained Components

Flux v5 treats component `body()` functions as mount-time declarations. A component is mounted into a `SceneNode` subtree once, and reactive values update retained nodes through bindings and effects after that.

## Component Contract

- A component can expose `Element body() const`.
- Advanced components can expose `std::unique_ptr<SceneNode> mount(MountContext&) const`.
- Hooks such as `useState`, `useComputed`, `useEffect`, `useAnimation`, and `useEnvironment` must run while a reactive owner scope is active.
- Reactive changes should flow through `Signal`, `Computed`, `Bindable`, control-flow views, or explicit scene-node effects.

## Retention Model

The retained scene tree is the identity layer. Flux does not re-run arbitrary component bodies to discover changes. Instead:

- `Bindable<T>` modifier values install effects against mounted scene nodes.
- `For` keeps keyed row scopes alive across reorder and disposes removed rows.
- `Show` and `Switch` own branch scopes and replace only the branch scene subtree.
- `EnvironmentLayer::setSignal` lets ambient values such as `Theme` participate in reactive tracking.

## Authoring Guidance

Prefer small components with stable inputs and reactive values captured by bindings:

```cpp
struct Swatch {
  Reactive::Signal<bool> active;

  Element body() const {
    return Rectangle{}
        .size(32.f, 32.f)
        .fill(Reactive::Bindable<Color>{[active = active] {
          return active.get() ? Color::accent() : Color::separator();
        }})
        .cornerRadius(8.f);
  }
};
```

Use control-flow views when the shape of the retained subtree changes. Use bindable modifiers when only node properties change.
