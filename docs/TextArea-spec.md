# TextArea — implementation spec (aligned with flux-v4)

This document matches the behaviour and data structures in [`include/Flux/UI/Views/TextArea.hpp`](../include/Flux/UI/Views/TextArea.hpp) and [`src/UI/Views/TextArea.cpp`](../src/UI/Views/TextArea.cpp). Section numbers are stable references for reviews.

## §1 — `LineMetrics` and coordinate space

`detail::LineMetrics` ([`TextEditKernel.hpp`](../include/Flux/UI/Views/TextEditKernel.hpp)) holds **layout-space** geometry:

- `top`, `bottom`, `baseline`: Y in the same space as `PlacedRun::origin` for the laid-out text (origin at the layout box top-left, Y down).
- `lineMinX`: minimum ink-left X on that visual line (layout space).

**Canvas-space** positions are obtained at call sites by adding the draw origin, e.g. `textOrigin.y + lineEntry.top` for selection rects. `buildLineMetrics` does **not** take `textOrigin`; it never mixes canvas and layout coordinates.

## §2 — Per-line UTF-8 ranges (`VisualLine` / `LineRange`)

Soft-wrapped lines require byte ranges from Core Text, not inference from glyph runs alone.

- `TextLayout::LineRange` is the canonical type ([`TextLayout.hpp`](../include/Flux/Graphics/TextLayout.hpp)). `TextLayout::VisualLine` is a `using` alias for `LineRange` (same type).
- Core Text fills `TextLayout::lines` in the `CTLine` loop using `CTLineGetStringRange`, mapped to UTF-8 byte offsets, with layout-space geometry aligned to the same transforms as `runs`.
- `PlacedRun` also carries `utf8Begin` / `utf8End` for each run.
- `buildLineMetrics` reads `layout.lines` when non-empty; otherwise it falls back to grouping runs by `ctLineIndex`.

## §4 — Caret memo and width

Caret geometry may be cached with `useMemo` in `body()` using a **provisional** content width: `useLayoutRect()` when available, otherwise `Runtime::buildSlotRect()` minus horizontal padding (same class of one-frame staleness as `TextInput`’s `frameX` pattern). Alternatively, caret caches can be computed in `render()` and passed as plain fields, mirroring `TextInput`.

## §5.4 — Vertical arrow keys (↑ / ↓)

The key handler needs a **layout-space X** for the caret column when moving to an adjacent line. Implementation:

1. Build the same layout box as editing: `contentWidth = fieldW - 2*paddingH`, `contentHeight = fieldH - 2*paddingV` (with `fieldW` / `fieldH` from `useLayoutRect()` or `buildSlotRect()` in `body()`).
2. Compute **line caret X** at the current byte from `buildLineMetrics` + `caretXPosition` + `lineMinX`.
3. **Preferred column (sticky X):** maintain `stickyCaretActive` + `stickyCaretLayoutX` (`useState` in [`TextArea.cpp`](../src/UI/Views/TextArea.cpp)). If inactive, use line caret X as the target; if active, use the stored preferred X (macOS-style column memory across lines of different widths). After each vertical move, set the preferred X to the **target** X used for hit-testing (not necessarily the new caret’s ink X after clamping to line end). `updateStickyColumnFromCaret` recomputes layout-space X from the current byte after horizontal moves, typing, pointer actions, etc.
4. Hit-test on the adjacent line with `caretByteAtPoint` using `Point{ targetX, midY }` in layout/content coordinates.

Horizontal navigation, typing, pointer placement, and other non-vertical caret moves **update** the preferred column from the new caret position.

## §6.4 — Scroll (`onScroll`)

The scroll handler may clamp `scrollOffsetY` by laying out the current text in the same content rect as `render()` (`ts.layout` with the wrapped box), then using `layout->measuredSize.height` as `contentH` and `fieldH - 2*paddingV` as the viewport height. Alternatively, omit clamping in `onScroll` and rely on render-time scroll reconciliation (possible one-frame overscroll).

## Summary

| Topic | Mechanism |
|-------|-----------|
| Line byte ranges | `TextLayout::LineRange` / `VisualLine`, filled in Core Text |
| `LineMetrics` space | Layout space; add `textOrigin` only when drawing |
| ↑ / ↓ column | `caretLayoutX` + optional sticky preferred X |
| Scroll clamp | Layout in handler or defer to render reconciliation |
