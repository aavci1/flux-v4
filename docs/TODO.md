- Full path drawing
- ~~Document layout process from end to end. How does measure and build work? How to implement a new component and make it work in the layout system? How to implement a new layout?~~ → See `docs/layout-system.md`
- Checkbox demo
- Rewrite, clean up all layout mechanism and the components

## Layout system overhaul (`layout-system-overhaul`)

| Item | Status |
|------|--------|
| A. `FLUX_ELEMENT_MODEL` macro (reduce `Element.hpp` boilerplate) | Done |
| B. `ContainerBuildScope` / `ContainerMeasureScope` | Done |
| C. Debug assertions (`LayoutEngine`, constraint stack, `Element::measure`, flex warnings) | Done |
| E. `FLUX_DEBUG_LAYOUT` stderr tree (constraints, measured, frame, flex) | Done |
| **D. Decouple stack alignment from `LayoutConstraints`** (`LayoutHints` + `BuildContext::hints()`, measure/build threading) | Done |
| **Variadic `children()`** (avoid `initializer_list` copies for `vector<Element>`) | Done |
| F–H. Larger follow-ups (see `docs/friction-points.md`) | Not started |

**D — intent:** `hStackCrossAlign` / `vStackCrossAlign` are not size constraints; they belong beside the constraint stack as `LayoutHints` so `LayoutConstraints` stays numeric bounds only.
