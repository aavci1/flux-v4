# Declarative UI: `body()` view trees

Conventions for the `auto body() const` member that returns a Flux view (designated-initializer tree). See [`examples/stack-demo/main.cpp`](../examples/stack-demo/main.cpp) for a full, formatted example.

## Indentation

- Use **2 spaces** per nesting level (same as the rest of the project; see [`conventions.md`](conventions.md)).
- **`return ViewType{`** sits one indent level inside `body()`.
- **Fields** of a view (`.spacing`, `.children`, etc.) are indented **one level** past the line that opens that view’s `{`.
- Inside **`.children = {`**, each sibling is indented **one more level** than the `.children` line so the hierarchy reads as a clear staircase.

## Braces and types

- Prefer **`ViewType {`** (space before `{`) for multi-line trees.
- **Designated initializers:** one **`.member = value`** per line; keep short nested aggregates on one line when readable (e.g. `.font = {.size = …, .weight = …}`).
- **Trailing commas** after the last field in a multi-line initializer are fine and match common struct style.

## Structure

- **Root** is often a scroll container (`ScrollView`, etc.) with layout fields (axis, flex) and a single **`.children`** entry that is the main stack.
- **Stacks** (`VStack`, `HStack`, `ZStack`): set spacing, alignment, padding, then **`.children = {`** and list siblings in visual order.
- **Nesting:** each inner `VStack` / `HStack` is a full indented block at the depth of its siblings—do not outdent inner stacks so they look like they leave the parent’s `children` array.

## Wrapping and modifiers

- Use **`Element { … }.withFlex(…)`** (and similar) when the inner view needs layout modifiers: put the inner view’s fields one indent inside `Element {`, and keep **`.withFlex(…)`** on the closing `}` before the comma.

## Colors and `main`

- **Shared colors:** a small **`namespace pal { constexpr Color … }`** (or equivalent) at file scope; reference as **`pal::name`** in views.
- **`main`:** `Application` → **`createWindow<Window>({ … })`** with window fields indented consistently → **`setView<RootType>()`** → **`app.exec()`**.

## Includes

- Include **`Flux/UI/UI.hpp`** (and any view headers you use: `VStack.hpp`, `Text.hpp`, etc.). Prefer the same include order as sibling examples: umbrella/core, then UI, then views.
