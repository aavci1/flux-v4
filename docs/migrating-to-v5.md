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
        .fill([active] {
          return active() ? Color::accent() : Color::separator();
        }})
        .cornerRadius(8.f)
        .onTap([active] { active = !active(); });
  }
};
```

When text or layout values must keep changing after mount, use a view or modifier with a reactive binding, or use a control-flow view that owns the changing subtree.

## State And Effects

- Use `useState(initial)` for local reactive state. It returns `Signal<T>`.
- Use `useComputed(fn)` for derived values.
- Use `useEffect(fn)` for side effects attached to the current owner scope.
- Use `onCleanup(fn)` when a branch, row, or component owns an external resource.
- Use `untrack(fn)` to read without subscribing.
- Use `signal()` as the canonical read-and-subscribe form. Use `signal.peek()` only for an intentional non-tracking read.

## Control Flow

- Use `For(signal, keyFn, rowFactory)` for keyed lists.
- Use `Show(conditionSignal, thenFactory, elseFactory)` for binary branches.
- Use `Switch(selectorFn, cases)` for multi-branch selection.

Control-flow factories receive their own scopes, so row and branch state persists exactly as long as the mounted subtree exists.

## Theme

Read the active theme with:

```cpp
auto theme = useEnvironment<Theme>();
```

Environment values are signals. Read `theme()` inside a `Bindable` closure or `Effect`
body when the result must update after `Window::setTheme(...)` or another environment
write. Reads at body time are one-time static reads, which is appropriate for fixed
mount-time layout seeds.

```cpp
Text {
  .text = "Status",
  .color = [theme] { return theme().labelColor; },
}
```

Debug builds warn when a signal-backed environment value is read outside a tracking
context, because that read does not subscribe to future environment changes.
