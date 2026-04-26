# Flux v4 - composite structural equality

Flux retains and reuses scene subtrees by comparing the `Element` tree produced by a component's
`body()` against the previous frame. Every type stored in an `Element` must therefore be
structurally comparable.

## Required contract

Any component, view, or measured leaf used as an `Element` value must either:

- Be trivially copyable.
- Define `operator==`.

Values pushed with `.environment(value)` must also define `operator==` and be copy-constructible.

The framework enforces this with a compile-time error in `Element::Model<C>`. This is intentional:
without a real comparison, `Element::structuralEquals` would return false for that value on every
frame and prevent subtree reuse above it.

## Equality rules

- If all fields are equality-comparable, prefer `bool operator==(T const&) const = default;`.
- For `Element` fields, compare with `element.structuralEquals(other.element)`.
- For `std::vector<Element>`, compare with `elementsStructurallyEqual(lhs, rhs)`.
- For `std::function` callbacks, compare callback presence with `static_cast<bool>(callback)`.
- For `State<T>` and animation handles, compare handle identity; the underlying value may change
  without requiring the view tree structure to change.
- For opaque fields such as `std::any`, compare only the structural information that affects
  retained-scene safety, and document the looseness near the comparison.

Callback values should not be compared directly. Flux refreshes retained interaction callbacks
through callback cells, so a retained node can keep a stable forwarder while the closure behind it
is updated on the next visited frame.

## Example

```cpp
struct Row : flux::ViewModifiers<Row> {
  std::string title;
  flux::State<bool> selected;
  std::function<void()> onTap;

  bool operator==(Row const& other) const {
    return title == other.title &&
           selected == other.selected &&
           static_cast<bool>(onTap) == static_cast<bool>(other.onTap);
  }

  flux::Element body() const;
};
```
