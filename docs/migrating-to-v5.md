# Migrating To Flux v5

Flux v5 is a hard cutover to retained mounting and fine-grained reactivity. Code should move data changes into signals and bind mounted scene-node properties to those signals.

## Component Shape

Keep `body()` as a declarative mount description:

```cpp
struct ToggleSwatch {
  Element body() const {
    auto active = useState(false);

    return Rectangle{}
        .size(48.f, 48.f)
        .fill(Reactive::Bindable<Color>{[active] {
          return active.get() ? Color::accent() : Color::separator();
        }})
        .cornerRadius(8.f)
        .onTap([active] { active = !active.get(); });
  }
};
```

When text or layout values must keep changing after mount, use a view or modifier with a reactive binding, or use a control-flow view that owns the changing subtree.

## State And Effects

- Use `useState(initial)` for local reactive state.
- Use `useComputed(fn)` for derived values.
- Use `useEffect(fn)` for side effects attached to the current owner scope.
- Use `onCleanup(fn)` when a branch, row, or component owns an external resource.
- Use `untrack(fn)` to read without subscribing.

## Control Flow

- Use `For(signal, keyFn, rowFactory)` for keyed lists.
- Use `Show(conditionSignal, thenFactory, elseFactory)` for binary branches.
- Use `Switch(selectorFn, cases)` for multi-branch selection.

Control-flow factories receive their own scopes, so row and branch state persists exactly as long as the mounted subtree exists.

## Theme

Read the active theme with:

```cpp
Theme const& theme = useEnvironment<Theme>();
```

For reactive theme-dependent values, use `themeField(&Theme::space3)` or capture the `EnvironmentValue<Theme>` in a bindable closure.
