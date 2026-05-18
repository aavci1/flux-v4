**Compositor Bug Sweep Status**

Resolved in `b6a8aac`, `e93908e`, and `d3a1bd7`.

- Popup hit-testing no longer allocates vectors per `popupScreenBounds` call.
- Popup and chrome hit tests share a single chrome-hit context instead of rebuilding popup bounds and scanning windows repeatedly.
- Surface role state is now represented with a single `SurfaceRole` tag instead of mutually inconsistent `toplevel`, `popup`, `subsurface`, and `cursor` booleans.
- Popup dismissal uses `XdgPopup::dismissed` as the visibility source of truth instead of mutating toplevel state.
- Snap, maximize, restore, and activation paths now require an xdg-toplevel role.
- `Super+Q` dismisses an active popup before closing the parent toplevel.
- `xdg_toplevel.configure` now advertises `activated`, `maximized`, and `resizing` states.
- xdg resource creation paths now check allocation failure before installing implementations.
- Initial toplevel placement now wraps within output bounds instead of cascading indefinitely off-screen.
- Command launcher text input uses xkbcommon UTF-8 translation instead of a hard-coded US keyboard table.
- Command launcher spawning uses `execvp` with parsed argv instead of `/bin/sh -lc`.
- Config color parsing accepts `#rgb`, `#rgba`, `#rrggbb`, and `#rrggbbaa`.
- Stale quote stripping was removed from config parsing.
- The hardcoded `compositor-sizes.log` debug path and unconditional render-size stderr spam were removed.
- Config hot reload only resizes the canvas when scale changes.
- The compositor cursor theme loader no longer depends on X11/libXcursor; it parses Xcursor files directly.
- Color shorthand and alpha parsing are covered by compositor config tests.
