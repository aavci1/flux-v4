# Reactive Graph

Flux v5 uses a fine-grained reactive graph under `Flux/Reactive`.

## Primitives

- `Reactive::Signal<T>` stores a value and notifies dependents when `set()` changes it.
- `Reactive::Computed<T>` lazily derives a value and tracks every signal read during evaluation.
- `Reactive::Effect` runs a side-effecting closure and re-runs when tracked dependencies change.
- `Reactive::Scope` owns effects, nested scopes, and cleanup callbacks.
- `Reactive::Bindable<T>` stores either a constant or a closure that can be evaluated inside an effect.

The `Flux/Reactive/Reactive.hpp` umbrella also exports convenient aliases in namespace `flux`: `Signal<T>`, `Computed<T>`, `Effect`, `Scope`, `makeComputed`, `withOwner`, `onCleanup`, and `untrack`.

## Ownership

Every mounted root owns a root `Scope`. Component mounts create child scopes, and control-flow views create branch or row scopes. Destroying a scope disposes its effects and runs registered cleanup callbacks.

```cpp
Reactive::withOwner(scope, [&] {
  auto count = useState(0);
  useEffect([count] {
    (void)count.get();
  });
});
```

## UI Bindings

Element modifiers accept constants and `Bindable<T>` values. During mount, Flux installs effects that evaluate bindables and apply the resulting value to the retained scene node.

```cpp
auto width = useState(120.f);

return Rectangle{}
    .size(Reactive::Bindable<float>{[width] { return width.get(); }},
          Reactive::Bindable<float>{24.f})
    .fill(Color::accent());
```

## Environment

`EnvironmentLayer` can hold constants or reactive signals. `useEnvironment<T>()` returns an `EnvironmentValue<T>` that preserves existing reference-style reads and also supports reactive reads through `operator()`.

`Window` owns a reactive `Theme` signal. Calling `Window::setTheme()` updates retained theme-dependent bindings without remounting the app.
