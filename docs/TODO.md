- Full path drawing

## Layout system overhaul (`layout-system-overhaul`)

| H. Separate layout pass from scene graph emission | Done — `LayoutTree` + `LayoutContext` (Phase 1) and `renderLayoutTree` + `RenderContext` (Phase 2); `BuildContext` removed; unit tests in `tests/` |

- Allow setting up model parameters. Save parameters along with the chat message so next message uses the sames params even when loaded from databsae

- Allow searching chats and within the chat

- cmd+q doesn't work

- Add an edit mode, display delete buttons only then

| Component | Notes |
|-----------|--------|
| **Alert** | `body()` |
| **Popover** | `body()` |

### **Primitive — `layout` / `measure` / `renderFromLayout` (default `Element::Model<C>`)**

| Component | Notes |
|-----------|--------|
| **Image** | Namespace `flux::views::Image` ([`Image.hpp`](include/Flux/UI/Views/Image.hpp)) |
| **Line** | |
| **Rectangle** | |
| **PathShape** | |
| **ScaleAroundCenter** | |
| **PopoverCalloutShape** | |