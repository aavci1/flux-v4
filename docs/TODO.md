- Full path drawing
- ~~Document layout process from end to end. How does measure and build work? How to implement a new component and make it work in the layout system? How to implement a new layout?~~ → See `docs/layout-system.md`
- Checkbox demo
- Rewrite, clean up all layout mechanism and the components

## Layout system overhaul (`layout-system-overhaul`)

| Item | Status |
|------|--------|
| A. `PrimitiveComponent` + unified `Element::Model<C>` (no per-type `Model` specializations) | Done |
| B. `ContainerBuildScope` / `ContainerMeasureScope` | Done |
| C. Debug assertions (`LayoutEngine`, constraint stack, `Element::measure`, flex warnings with `FLUX_DEBUG_LAYOUT`) | Done |
| D. Decouple stack alignment from `LayoutConstraints` (`LayoutHints` + `BuildContext::hints()`, measure/build threading) | Done |
| **Variadic `children()`** (avoid `initializer_list` copies for `vector<Element>`) | Done |
| E. Layout diagnostics (`FLUX_DEBUG_LAYOUT` stderr tree; **⌘⇧L** — Meta+Shift+L — toggles visual layout bounds overlay via `Runtime::layoutOverlayEnabled()`) | Done |
| | |
| F. Single leaf bounds resolver (`resolveLeafLayoutBounds`; former `resolveRectangleBounds` / `isRectangle` split removed) | Done |
| G. Further reduce `Element.hpp` includes (e.g. move `RenderComponent` graph emission behind a boundary) | Not started |
| H. Separate layout pass from scene graph emission | Not started (see `docs/friction-points.md`) |

**D — intent:** `hStackCrossAlign` / `vStackCrossAlign` are not size constraints; they belong beside the constraint stack as `LayoutHints` so `LayoutConstraints` stays numeric bounds only.
