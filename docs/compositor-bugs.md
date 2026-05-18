**Issue 1: `popupScreenBounds` allocates two vectors per call.** Look at lines 121, 143. Every pointer motion event calls `surfaceAt`, which iterates all popups and calls `popupScreenBounds` for each. That's two `std::vector` allocations *per popup per pointer event*. On a 60Hz pointer with 2 popups open, that's 240 heap allocations per second just for hit-testing. This isn't a correctness problem but it's gratuitous heap churn in a hot path. The accumulation should be done with a fixed-size array or scalar offsets — there's no algorithmic reason to materialize the chain as a vector.

**Issue 2: `surfaceAt` rebuilds the popup bounds twice per click.** Note that `surfaceAt` calls `popupScreenBounds` for each popup. Then `titlebarAt`, `closeButtonAt`, `resizeGripAt` each call `popupContainsPoint`, which again iterates all popups calling `popupScreenBounds`. A single pointer event hits popup bounds computation 4× in sequence. With multiple popups open, the multiplier compounds.

**Issue 3: `surfaceAt` checks `!surface->toplevel || surface->popup` (line 193).** A surface with both `toplevel=true` and `popup=true` is impossible, but the conjunction reads as defensive against an impossible state. Either the type system should make this exclusive (an enum, not two bools), or one check is redundant. Looking at the `Surface` struct earlier — `toplevel` and `popup` are separate booleans. The right model is one variant tag, not two redundant flags.**Issue 4: massive structural duplication across `titlebarAt`, `closeButtonAt`, `resizeGripAt`.** All three functions:

1. Check `popupContainsPoint` first.
2. Iterate `server->surfaces_` in reverse.
3. Skip popup surfaces.
4. Handle the "non-managed front toplevel blocks back chrome" case identically.
5. Then do their specific test.

Lines 203-227, 229-256, 258-292 are 50%+ identical scaffolding. A `ChromeHitContext` helper that does the iteration and yields managed-toplevel-rect-with-occlusion-check, then per-function only the specific hit math.

**Issue 5: `updateCompositorCursorForPointer` calls `closeButtonAt` then `resizeGripAt` on every pointer motion** (line 316, 321). Each of those iterates all surfaces and checks popup containment. The chain is: pointer motion → cursor update → close-button hit test → popup containment (iterate all popups) → surface iteration (iterate all surfaces) → repeat for resize grip. On a 120Hz pointer with 5 windows and 2 popups, this is hundreds of redundant iterations per second.

Now let me look at the resize/drag state machine:**Issue 6: `dismissPopup` mutates the surface's `toplevel` field** (line 424). This is alarming. A popup surface had `toplevel=false` (correct), but if it gets set to false again here it suggests the flag was being used as something other than "is this a toplevel." Let me check what the flag means more carefully:The Surface has separate `toplevel` and `popup` boolean fields, plus `layerSurface` and `subsurface`. And **`LayerShell.cpp:97` sets `toplevel = true`** for a layer surface — but layer surfaces aren't toplevels. And **`XdgShell.cpp:429`** (which I'll bet is the popup creation path) sets `surface->toplevel = true` even though it's creating a popup.

Let me verify:**Confirmed.** At popup creation time (line 429), the popup surface gets `toplevel = true` *and* `popup = true`. This explains the cryptic check in `surfaceAt` at line 193:

```cpp
if (!surface || !surface->toplevel || surface->popup || width <= 0 || height <= 0) continue;
```

The check **isn't redundant** — it's compensating for the fact that popups have `toplevel = true`. The compositor uses `toplevel` to mean something like "this surface has committed content and should be displayed," and `popup` is a separate flag for "this is a popup."

So the bool flags are semantically tangled:
- `toplevel = true` means "renderable surface with content"
- `popup = true` means "is a popup (which also has `toplevel = true`)"
- `layerSurface = true` means "is a layer surface (which also has `toplevel = true`)"
- A subsurface presumably has `toplevel = false` and `subsurface = true`

**This is a real design bug.** The `toplevel` flag is overloaded — it's both "is an xdg_toplevel" semantically and "should be rendered" operationally. The same flag means two different things, and code that wants the first meaning has to write `surface->toplevel && !surface->popup && !surface->layerSurface && !surface->subsurface`. That's exactly what `isManagedToplevel` is (line 36):

```cpp
bool isManagedToplevel(WaylandServer::Impl::Surface const* surface) {
  return surface && surface->toplevel && !surface->popup && !surface->layerSurface && !surface->subsurface;
}
```

**Issue 7: surface role is encoded in mutually-non-exclusive booleans where it should be a tagged enum.** The right model is:

```cpp
enum class SurfaceRole : std::uint8_t {
    None,
    XdgToplevel,
    XdgPopup,
    LayerSurface,
    Subsurface,
    Cursor,
};

struct Surface {
    SurfaceRole role = SurfaceRole::None;
    bool hasCommittedContent = false;  // what `toplevel = true` actually meant operationally
    // ...
};
```

Then `isManagedToplevel` becomes `surface->role == SurfaceRole::XdgToplevel`. The confusing four-way conjunction goes away. The `dismissPopup` line that sets `toplevel = false` becomes `role = SurfaceRole::None` or whatever the intent was — and you can grep for "what state is a dismissed popup supposed to be in" cleanly. Right now you can't tell without reading multiple files.

**Issue 8: `dismissPopup` setting `toplevel = false` is a side-channel** to make the popup invisible after dismissal. The popup is still in `server->popups_` (until destroyed) but `toplevel = false` makes it skip rendering. **This conflates "should be rendered" with "is a toplevel role".** Two separate concepts in one field.

The right shape: dismissed popups have `popup->dismissed = true` (which already exists, line 471 of `WaylandServerImpl.hpp`). The renderer should check `dismissed` directly, not via the side effect on `toplevel`.

Let me check the renderer:**Confirmed: the snapshot path checks `surface->toplevel` (line 93) which actually filters out subsurfaces, layer surfaces' contents, etc.** The flag means "should be considered as a top-level renderable" — i.e., subsurfaces get rendered via `appendSubsurfaceSnapshots` instead.

So `toplevel` is really "is a top-level (non-subsurface) renderable thing." And `dismissPopup` sets it to `false` to hide the popup from the renderer. But there's also `dismissed` which is *also* checked at line 94. **Belt-and-suspenders that disagree**: line 94 wouldn't be needed if dismissed popups had `toplevel = false`. So which is the source of truth?

Either of:
1. `toplevel = false` is the source; the `dismissed` check at line 94 is dead.
2. `dismissed` is the source; the `toplevel = false` mutation in `dismissPopup` is a side effect that should be removed.

Without seeing the exact data flow it's hard to say, but **two ways of expressing the same state will eventually disagree.** This is real correctness debt.

Let me look at command launcher next, since that's the largest new code:**Issue 9: hand-rolled US-keyboard layout translation in `commandLauncherCharForKey`.** Lines 748-803 hand-decode raw evdev keycodes to ASCII assuming US QWERTY layout. This is incorrect for any non-US user. Worse — Flux already integrates with libxkbcommon (which Wayland keyboard mapping uses) and the compositor uses xkb for keyboard layout (search confirms it):So xkbcommon is already loaded with the user's keymap. The launcher should use `xkb_state_key_get_utf8` or `xkb_state_key_get_one_sym` instead of the hand-rolled switch. The current code is **a 56-line layout-incorrect substitute for a 1-line library call**. For users on AZERTY, Dvorak, Turkish-Q (likely relevant to you specifically given you're in Ankara), or any non-US layout, the launcher accepts wrong characters.

**Issue 10: `spawnCommand` does the double-fork dance** (lines 805-822) to detach the child from the compositor process group. That part is correct — without it, the spawned terminal would be a child of the compositor and could be killed accidentally. **But it does `execl("/bin/sh", "sh", "-lc", command.c_str(), ...)`** — running through `/bin/sh -lc` means shell injection if the user enters anything. Probably fine for a single-user dev system; would be a vulnerability in a multi-user environment.

Also, the parent reaps the first-stage child with `waitpid` (line 820-821), which is fine, but the parent does this **synchronously on the compositor thread**. The fork() itself is fast but if anything in the prep takes time, the compositor blocks. In practice for a typical command this is microseconds; just noting.

Let me look at config parsing now:**Issue 11: `parseHexColor` only accepts 7-char `#rrggbb`.** No support for `#rrggbbaa`, `#rgb`, or `#rgba` shorthand. Standard CSS colors. If a user writes `#fff` in their config, it silently fails to parse and the field stays default. Should accept all four lengths and provide a clear error on a malformed value.

**Issue 12: No alpha in color parsing.** The compositor's renderer almost certainly supports alpha (it has shadows, transparency, etc.), but the config can only set opaque colors. If you want a semi-transparent overlay color from config, you can't. Minor for now; revisit when transparency is desired in config.

**Issue 13: `unquote` is called by `parseHexColor` and `parseLinearGradient`** despite the fact that the values are coming from tomlplusplus, which **already strips quotes for string values**. The unquoting here is dead work — TOML strings are stripped of their quotes by the parser. Unless these are being fed from somewhere else, this is residual code from when the parser was hand-rolled. Let me check usage:Confirmed. `configString` reads a TOML string via tomlplusplus (which has already stripped the quotes), and then `parseHexColor` calls `unquote` on it. The `unquote` is a no-op now. Same for `trim` — TOML strings don't have leading/trailing whitespace either. **Dead code from the hand-rolled-parser era.** ~30 lines of trim/unquote/etc. that's all unreachable in practice.

Let me skim the remaining concerns in WindowManager and then look at a different module:**Issue 14: `focusedToplevel` falls back to popup->parent.** Line 512-514. When the keyboard focus is on a popup, "focused toplevel" returns the popup's parent toplevel. This is mostly right, but combined with `closeFocusedToplevel` (lines 518-523), pressing the close shortcut while a popup is up will close the *parent* of the popup, not the popup itself. That's probably right (popups don't have separate close), but it's an interaction worth checking — Super+Q while foot's right-click menu is open would close the entire terminal. Possibly unexpected.**Issue 15: `snapToplevel`/`maximizeToplevel`/`restoreToplevel` check `surface->toplevel`** (lines 581, 600, 618). Same role-confusion as Issue 7. A popup surface has `toplevel=true`, so these checks pass for popups. They'd then mutate `surface->snapped` / `surface->maximized` on a popup, which is wrong. Probably doesn't happen in practice because the callers come from shortcut handling where focused surface is usually a toplevel, but the guards are wrong for what they should be — should be `isManagedToplevel` not raw `toplevel`.

Let me check `sendToplevelConfigure`:**Issue 16: `wl_array states` sent empty in toplevel configure** (line 55-58). xdg-shell's `xdg_toplevel.configure` accepts a list of states (`maximized`, `activated`, `fullscreen`, `resizing`, etc.). The compositor never tells clients about state. Why does this matter?

- **Clients that visually adapt to "I'm maximized"** (e.g., browsers hiding window decorations when maximized) get no signal. They render as if always windowed.
- **`activated` state** is how a client knows it has keyboard focus. Without it, clients can't show "active" vs "inactive" window styling. foot probably uses this for the cursor blink state.
- **`resizing` state** lets clients optimize during interactive resize (e.g., skip rendering full content).

This is a real protocol gap. xdg-shell requires the compositor to send these. The compositor's status text says "send_configure does not include states" — actually wait, let me check:No XDG_TOPLEVEL_STATE_* values are ever sent. Confirmed. Real protocol bug.

Let me look at Selection.cpp briefly since it's 569 lines:Selection.cpp looks substantially more correct than my other findings — this code understands DnD action negotiation, version-gates protocol calls properly (`wl_resource_get_version(...) >= WL_DATA_OFFER_ACTION_SINCE_VERSION`), and handles invalid action masks with proper error posting. Real-feeling Wayland code.

Now let me check the XdgShell file:**Issue 17: Initial window placement at fixed cascade `(80 + N*36, 80 + N*36)`.** Lines 276-277. The Nth opened window starts at offset 80+36N from the previous one. This is a perfectly fine fallback, but:
- It doesn't reset when windows close (it uses `toplevels_.size()` — sticking a new window at slot N when there are N toplevels currently, but if the user has closed 5 and opened 5, you're at slot 5 even though slots 0-4 are empty).
- Past output bounds the windows cascade off-screen. On a 1080p output, the 30th window starts at (1160, 1160) — partially off-screen.
- Clamp to output bounds and find a free slot would be more robust.

Not urgent; "casual fallback positioning" is the spec.

**Issue 18: `wl_resource_create` on line 278 has no null check.** If the resource creation fails (out of memory, version mismatch), `toplevel->resource` becomes nullptr, then `toplevel` gets pushed into `toplevels_`, then `wl_resource_set_implementation` is called on `nullptr` which will crash. Most other paths in this file do check (e.g., `Activation.cpp`, `Selection.cpp`). This one doesn't. Real crash on resource creation failure.**Issue 19: Initial configure sends size `(0, 0)`** (line 291). For xdg_toplevel, configure(0, 0) tells the client "size yourself however you want." This is the right interpretation per xdg-shell spec. **But** the empty `wl_array states` is the same gap as Issue 16 — the client never receives `XDG_TOPLEVEL_STATE_ACTIVATED` when it first opens, even if it should have focus. The client appears as unfocused on open.

Let me look at the renderer:**Issue 20: `appendSizeLog` opens-writes-closes a file PER CALL** (lines 30-38). Logging to `compositor-sizes.log` opens the file, writes, closes it. Each frame, potentially. This is a syscall storm. If `appendSizeLog` is called per surface per frame on a 60Hz display with 5 surfaces, that's 1800 open/close pairs per second. A file kept open with periodic flush would be one syscall per call.

Let me see where it's used:**It's gated on `snapshotChanged`** (line 177). So per surface, per frame where the surface changed shape, opens-writes-closes the log. During interactive resize this fires every frame. Still many syscalls. And — **`compositor-sizes.log` is left in the current working directory**. Not in `~/.cache`, not in `/tmp`, not configurable. Wherever the compositor is run from. **Hardcoded path in the cwd.** This is leftover debug code, not production code.

Compare to `flux::detail::resizeTrace` which uses an env var and is properly gated. This `appendSizeLog` predates the unified tracing utility and was never cleaned up.

The same dual-logging pattern from before. Lines 199-218 (continuing) write the same payload to stderr too:**Three logging mechanisms for one event:**
1. `appendSizeLog` → file `compositor-sizes.log` (always when snapshotChanged).
2. `std::fprintf(stderr, ...)` (always when snapshotChanged).
3. `detail::resizeTrace(...)` → env-gated tracing.

The first two are unconditional. They fire every snapshot change. **The stderr output will spam during normal use** — every window move, resize, configuration acknowledgment produces a log line. Open a few apps and watch the terminal scroll. This is production code logging at debug levels.

This is the same kind of "left in after debugging" debris as the resize-trace duplication that the cleanup spec already addressed. The cleanup unified the resize-trace utility but **didn't clean up this older parallel logging**. Three trace duplicates → one was fixed → two more turned up.

Now let me check the cursor and chrome modules briefly:**Issue 21: `#include <X11/Xcursor/Xcursor.h>` in a Wayland compositor.** Lines 5-8. Then `#ifdef CursorShape #undef CursorShape #endif` to undo a macro collision (X11 defines `CursorShape` as a macro). This is **pulling in X11 headers just for the cursor theme file format parser.**

Xcursor *files* are an X11 standard but the file format is decoded by `libXcursor`, which depends on `libX11`. That means `libX11` and probably `libxcb` are now compositor dependencies, in a project that explicitly chose "no X11/no XWayland" (spec §1.6).

Better alternatives:
- **wlroots' wlr-icon-helper** style: a small Xcursor file format parser without the X11 library dependency. The Xcursor format is ~100 lines of file-format code.
- **`libwayland-cursor`** comes with the Wayland project specifically for this purpose. It reads Xcursor files and provides cursor frames without pulling in X11. It's the standard solution for "Wayland compositor needs to read cursor themes." Already a probable dependency of the project (the demo clients use it).

The X11 dependency is unnecessary. **Replace `Xcursor` with `wayland-cursor`** to eliminate the X11 transitive dependency. Probably 50-100 LOC change, removes a dependency tree.

Let me look at CompositorRuntime briefly since it's the entry-point glue:**Issue 22: Hardcoded `compositor-sizes.log` in `applyConfig`** (lines 169-178). Same as the SurfaceRenderer case — opens, writes, closes a hardcoded file in cwd. **Plus the exact same line is duplicated immediately to stderr** (lines 179-185). The same info goes to two destinations every config apply.

The `applyConfig` lambda fires on config hot-reload, so it's much rarer than the per-frame logging — once per config save. Less of a perf issue, but still production logging at debug detail.

**Issue 23: Config hot-reload triggers a full canvas resize and DPI update** (lines 167-168) regardless of whether the relevant fields changed. If you change the wallpaper color, the canvas is resized to the same dimensions it already had. The DPI scale is set to the same value. Wasted work; harmless but wasteful.

A diff-based apply (only invalidate affected state when the relevant field changes) would be the right model, but is non-trivial. As-is is fine for early development.
