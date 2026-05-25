# Lambda Desktop daily-driver assessment

**Date:** 2026-05-25
**Scope:** `lambda-window-manager`, `lambda-shell`, and the current Lambda apps in this repository.
**Basis:** Static review of the current tree, existing docs, and the recent manual validation work. This is not a fresh full hardware test run.

## Summary

Lambda is now at a credible dogfooding stage: the compositor can own a KMS output, run Wayland clients, render shell panels, launch the shell, launch Flux apps, handle window movement/resizing/snapping, render glass backgrounds, and run a usable first version of `lambda-terminal`. That is a meaningful base for a personal desktop environment.

It is not yet daily-driver complete. The missing pieces are less about drawing windows and more about the surrounding desktop contract: session lifecycle, output management, app discovery, real settings backends, core file operations, terminal interaction basics, system status, clipboard integration in apps, notifications, power/network/audio controls, and robust real-app validation.

For a practical first daily-driver target, I would define v1 as:

- Pure Wayland, no XWayland requirement.
- One or two known machines, with hardware explicitly validated.
- `lambda-window-manager` launched by a simple session wrapper, not necessarily a display manager.
- `lambda-shell`, `lambda-terminal`, `lambda-files`, and `lambda-settings` usable for common work.
- External apps such as Firefox can be launched and managed.
- Basic system controls exist for display scale/output, wallpaper/glass/theme, keyboard shortcuts, input, power, audio, network, and screenshots.

The fastest route is not to build every desktop app first. The preferred working order is: Window Manager, Shell, Settings, Files, Terminal, then the remaining desktop pieces. Settings can remain manually edited during the Window Manager and Shell phases, but it should become real before Files and Terminal are treated as daily-driver readiness work. Browser, mail, calendar, and media can be mature Wayland apps until native Lambda versions are worth building.

## Current status by layer

### Flux framework

The v5 runtime is in good shape for building Lambda apps:

- Retained view mounting, signals, computed values, effects, bindable modifiers, and environment values are implemented.
- Mac, Linux Wayland/Vulkan, and Linux KMS/Vulkan backends exist.
- `Window::setBackground()` now expresses transparent, solid, gradient, and glass backgrounds through one API.
- macOS glass uses `NSVisualEffectView`; Wayland glass uses `ext_background_effect_v1` plus compositor rendering.
- Tests cover the reactive graph, mount/runtime behavior, layout, scene traversal, text editing utilities, theme reactivity, and selected app components.

Main gaps:

- Cross-backend parity is good for rendering, but the desktop environment itself is Linux/KMS-only.
- There is no high-level desktop services framework yet: portals, notifications, settings persistence, app registry, clipboard history, and power/session integration are outside Flux today.
- Visual/performance regressions are mostly validated manually with trace scripts rather than automated screenshot or animation tests.

### Lambda window manager / compositor

Implemented and working enough for dogfooding:

- KMS/DRM ownership from a TTY, with Vulkan rendering and a GBM/atomic-KMS presenter by default.
- Single selected output, configurable by connector name, index, `primary`, or `secondary`.
- Wayland server with `wl_compositor`, `wl_subcompositor`, `wl_shm`, `wl_output`, `wl_seat`, `xdg-shell`, xdg-decoration, linux-dmabuf, xdg-output, viewporter, cursor-shape, idle-inhibit, layer-shell, presentation-time, relative-pointer, pointer-constraints, primary-selection, data-device clipboard/DnD, fractional-scale, xdg-activation, `ext_background_effect_v1`, and `xx_cutouts_v1`.
- Stacking window management: focus, raise, move, resize, close, minimize, maximize, snap left/right/corners/top, snap preview, geometry animations, and keyboard shortcuts.
- Server-side window chrome, integrated titlebar cutouts, shadows, borders, rounded corners, and unified glass material rendering for titlebar/content.
- Shell IPC for snapshots, app launch/focus requests, and command launcher modal ownership.
- Built-in full-screen screenshot saving to `~/Pictures/Screenshots`.
- Config loading and hot reload for wallpaper/background, scale, cursor theme/size, output selector, animation toggle, idle blanking, keybindings, chrome metrics, border/shadow colors, and glass material.
- Diagnostic scripts and logs for CPU, pacing, resize, terminal rendering, terminal resize, and snap flicker.

Known compositor gaps and risks:

- Single active output only. Multi-monitor layout, per-output work areas, output hotplug, output positioning, and launcher placement per active output are not implemented.
- No display manager, greeter, PAM/login integration, lock screen, logout/reboot/suspend UI, or crash-recovery session wrapper.
- Input permissions are still manual unless the host session grants `/dev/input/event*` ACLs.
- DPMS/panel power-off is not implemented. Software idle blanking exists, and idle inhibitors are tracked.
- Adaptive sync and triple-buffering are still listed as remaining work.
- Popup support has demos, but broader GTK/Qt/browser popup behavior needs validation. Popup grabs are config-gated and not the default.
- Touch is not advertised, and touch/tablet/gesture support is not a daily-driver feature yet.
- Input methods are missing. This matters for non-US keyboard workflows, compose/dead keys beyond current xkb basics, CJK input, emoji input, and accessibility.
- No XWayland. This is acceptable as a product decision, but it restricts app compatibility.
- The compositor has protocol support for clipboard and drag/drop, but Lambda apps do not yet expose polished clipboard/DnD workflows.
- There is no security/portal layer for screenshots, screencast, file chooser, open-with, secrets, notifications, or permission prompts.
- Window management is stacking-only. There are no workspaces, overview, expose-style view, tiling mode, window switcher UI, or persisted window/session restore.
- Minimized windows are likely awkward from shell activation: the dock sends focus for a running app, while the current focus path skips minimized surfaces. Dock-click should restore or unminimize.
- Hardware validation is still narrow. External display testing was explicitly deferred after the latest snap-animation work.

### Lambda shell

Implemented:

- `lambda-shell` is a separate trusted process.
- It creates layer-shell surfaces for top bar, dock, and command launcher.
- The shell receives window-manager snapshots over Unix socket IPC.
- The top bar reserves work area.
- The dock shows pinned app items, running/focused dots from compositor snapshots, and launches/focuses apps through the compositor.
- `Super+Space` is captured by the compositor and routed to the shell command launcher.
- The command launcher filters the app registry and launches/focuses app results.
- Shell panels use the compositor-backed background effect path.

Main gaps:

- The app registry is hard-coded in the compositor. There is no `.desktop` parser, user pinning model, app categories, icon lookup, search ranking, aliases, recently used apps, or command provider.
- Dock and launcher icons are hard-coded Flux/Material glyphs with app-specific color palettes. There is no freedesktop icon theme lookup, symbolic icon support, app icon loading from desktop entries, MIME/file icons, or user-configurable icon theme.
- Dock contents are static. Browser, Calendar, Mail, Music, and Trash are present as shell entries, but most map to external commands or no useful native implementation.
- The top bar status area is mostly visual. The compositor snapshot currently reports system status fields as `"unknown"`, and the top bar draws static Wi-Fi/Bluetooth/volume/battery icons.
- No quick settings, calendar popover, notification center, system tray/status notifier, workspace switcher, or active app menu.
- The shell spec says it should tolerate window-manager restarts, but the current production shell throws if IPC connection fails and quits when IPC disconnects.
- The launcher is app-tile search only. It does not search files, commands, settings, open windows, recent documents, calculator expressions, or shell actions.
- Launcher modality exists, but a more complete focus/keyboard model and escape/outside-click behavior should be tested under real load.
- Multi-output shell behavior exists in the spec, not in the current implementation.

### lambda-terminal

Implemented:

- Native Flux terminal app based on a pty and libvterm.
- Launches `$SHELL` with `TERM=xterm-256color` and `COLORTERM=truecolor`.
- Receives pty output through the Flux event loop and requests redraws.
- Sends text input and common keys to the pty.
- Renders foreground/background colors, bold text, cursor, and a black glass window background.
- Handles resize by updating vterm size and `TIOCSWINSZ`.
- Recent work added row damage, row caching, fast ASCII layout, and trace scripts for terminal rendering/resize.

Daily-driver gaps:

- No scrollback.
- No selection, copy, paste, bracketed paste, or clipboard integration.
- No mouse reporting, URL detection/opening, search, tabs, splits, profiles, font configuration, color schemes, or configurable opacity/glass.
- Key handling is incomplete for function keys, many modifier combinations, Alt/Meta behavior, keypad keys, and application cursor/keypad modes.
- Unicode handling is likely incomplete. The renderer uses the first `VTermScreenCell::chars` entry, so combining sequences, complex emoji, and some wide-character cases need explicit validation and fixes.
- No terminal preferences UI and no persisted settings.
- No graceful shell-exit UX beyond a status line.
- Resize performance is much better, but it still needs a repeatable regression target and comparison against real terminal workloads.

### lambda-files

Implemented:

- Native Flux file manager window with integrated titlebar and glass background.
- Sidebar places for Home, Desktop, Documents, and Downloads when present.
- Directory listing with folders first, sorted case-insensitively.
- Grid layout, file/folder visual kinds, selection, subtitles, breadcrumbs, back/forward/up navigation, hidden-file toggle, and basic keyboard shortcuts.
- Opens files through `xdg-open` on Linux and `open` on macOS.

Daily-driver gaps:

- No create folder/file, rename, delete, trash, copy, move, duplicate, paste, or undo.
- No context menus, drag/drop, clipboard integration, multi-select, range select, or keyboard selection model.
- No list/detail/column views, sort controls, search, path entry, refresh button, or filesystem watcher.
- No thumbnails/previews, image/PDF preview, archive browsing, metadata inspector, permissions UI, or open-with chooser.
- No mounted volume/removable drive/network location support.
- Error handling is basic and mostly local to listing/opening.
- `xdg-open` via shell command is a useful bootstrap but should become a desktop open-with/portal service.

### lambda-settings

Implemented:

- Native Flux settings app with a standard/system titlebar and glass window background.
- Polished visual sections for General, Appearance, Desktop, Dock & Panel, Workspaces, Privacy, Notifications, Power, and About.
- Local reactive controls for theme mode, accent, wallpaper choice, transparency, radius, reduce motion, and high contrast.

This app is currently the clearest placeholder:

- Most values are mock data. Examples include hostname, update cadence, battery, display, kernel, storage, workspaces, privacy permissions, and power settings.
- Controls do not write to compositor config, shell config, app config, or system services.
- No persistence.
- No display/output backend despite compositor support for selected output and scale.
- No input/keyboard/touchpad settings.
- No real wallpaper picker, glass material editor, keybinding editor, cursor theme editor, icon theme editor, font settings, dock/panel settings, notification settings, power settings, audio settings, network settings, Bluetooth settings, users/accounts, default apps, or about/system info backend.

For daily use, Settings should stop being a mock as soon as possible. It is the natural control surface for the compositor config and shell preferences.

### Themes, icons, and cursors

Current status:

- Cursor themes work when an Xcursor theme is installed. The compositor already supports `cursor_theme` and `cursor_size` in config and falls back through environment/system defaults.
- Cursor configuration is not exposed in Settings yet.
- The shell and Lambda apps mostly use built-in icon glyphs and local visual mappings. This is fine for prototypes, but it is not enough for a desktop environment.

Daily-driver gaps:

- Add a shared icon theme service/provider that understands freedesktop icon themes, theme inheritance, common icon sizes, symbolic icons, and fallback lookup.
- Use that provider in shell dock, launcher, Files file-type icons, open-with/default-app UI, Settings, and future apps.
- Load app icons from desktop entries once the app registry moves out of hard-coded C++.
- Load MIME/file icons from the icon theme instead of app-local file-kind glyphs.
- Provide a bundled fallback icon/cursor theme or document a required package set so a fresh install is not visually broken.
- Expose cursor theme, cursor size, icon theme, and icon size in Settings.

### Other examples and prototype apps

The repository has many useful demos and prototypes: `lambda-shell-preview`, `lambda-studio`, `solitaire-app`, UI control demos, compositor protocol demos, and render fixtures. They are valuable development tools, but most should not be counted as desktop environment apps.

Notable distinction:

- `lambda-shell-preview` is a development preview for shell UI, not the production shell.
- Compositor demo clients validate protocols, not user workflows.
- `lambda-studio` is a substantial prototype app, but it is not part of the minimum desktop surface unless the goal expands to a built-in AI/workspace app.

## Placeholders and partial implementations

These are the areas most likely to look complete visually while still needing real behavior:

- Settings pages and values.
- Top-bar system indicators.
- Dock app registry and inactive app set.
- Calendar, Mail, Music, Browser, and Trash dock entries.
- Command launcher providers beyond app tiles.
- Shell reconnect/restart behavior.
- Icon theme lookup and app/file icons.
- Real app popup behavior.
- Minimized-window restore from shell.
- Screenshot UX beyond the global full-screen shortcut.
- Open-with/default-app handling.
- Clipboard and DnD inside Lambda apps.
- Multi-output behavior in shell and compositor.
- Session management and power actions.

## Daily-driver blockers

### P0 - Must fix before relying on it daily

1. Harden the Window Manager and session base.

   Finish the operational window-manager pieces first: known-good idle CPU, resize/snap visual stability, screenshot/trace tooling, selected-output behavior, input permissions, cursor theme loading, and a minimal `lambda-session` that launches `lambda-window-manager`, launch/relaunches `lambda-shell`, sets up logs, publishes environment, handles clean shutdown, and gives a reliable recovery path. It does not need to be a display manager on day one.

2. Make shell launch/focus/restore robust.

   Replace hard-coded app commands with an app registry. At minimum, support the local example executables, Firefox/other external Wayland apps, app IDs, focusing existing windows, restoring minimized windows from the dock, and loading app icons from the configured icon theme.

3. Turn Settings into a real control panel for current Window Manager and shell features.

   First useful backends: wallpaper/background, glass material, corner radius/borders/shadows, scale, selected output, keybindings, cursor theme/size, icon theme, font settings, animations, idle blanking, and shell dock/panel preferences.

4. Make Files safe for real file management.

   Implement trash-first delete, create folder, rename, copy/move, paste, multi-select, context menus, filesystem refresh/watch, and better error reporting. Avoid permanent deletion as the first implementation.

5. Finish terminal basics.

   Scrollback, selection, copy/paste, bracketed paste, more complete keys/modifiers, and Unicode/wide-character validation are required for a daily terminal.

6. Add core system controls.

   At minimum: volume, network status, battery/power state, brightness if relevant, suspend/reboot/logout, and a lock story. These can start as simple shell panels backed by system commands/services.

7. Validate with real Wayland apps.

   Firefox, at least one GTK app, at least one Qt app, and a mature terminal such as foot should be part of the smoke checklist. Popups, clipboard, drag/drop, fractional scale, and resize behavior should be explicitly checked.

### P1 - Needed for comfort and polish

- Multi-output layout and hotplug.
- Notification daemon and notification center.
- Quick settings panel from the top bar.
- Workspaces or an overview/window switcher.
- Clipboard history.
- Screenshot UI for full screen, area, and active window.
- `.desktop` indexing, icon theme lookup, default apps, MIME open-with, and recent documents.
- Better pointer/touchpad configuration.
- Input method support.
- Basic accessibility: keyboard navigation audit, contrast modes that actually apply, focus visibility, reduced motion, and eventual screen-reader hooks.
- Automated visual regression tests for glass, shadows, titlebars, panels, resize, and snap animations.

### P2 - Important, but can wait until v1 is usable

- XWayland, if app compatibility becomes more important than keeping the system pure Wayland.
- Built-in browser, mail, calendar, music, video, PDF, archive manager, and image viewer.
- System monitor/task manager.
- Screencast/recording and xdg-desktop-portal equivalents.
- Secrets/keyring integration.
- Installer/package/session-manager integration.
- Theme marketplace or advanced personalization.

## Recommended native app set

For a basic desktop environment, I would target this order:

1. Settings - already started; replace mock pages with real backends.
2. Files - already started; add safe file operations.
3. Terminal - already started; finish interaction fundamentals.
4. Screenshot - Window Manager capture exists; add a small UI for area/window/full-screen capture.
5. Text Editor - a preliminary native editor is more valuable early than native mail/music/calendar.
6. Document Viewer - PDF first, images if that is faster than a separate image viewer.
7. System Monitor - useful for killing hung apps and diagnosing the environment itself.
8. Archive Manager - common file workflow.
9. Calculator - small, easy, useful command-launcher provider too.

For browser, mail, calendar, and music, use mature Wayland apps first. The shell can still expose them as pinned apps. Native Lambda versions should come later unless they are part of the product identity.

## Suggested milestones

### Milestone 1 - Personal dogfood baseline

Goal: You can boot to a TTY, start Lambda, work for a few hours, and recover from mistakes.

- Window Manager idle/render/snap/resize stability validated on the target machine.
- `lambda-session` script/binary.
- Shell auto-start and shell restart on crash.
- Robust app launch/focus/restore with themed app icons.
- Real Settings page for Window Manager and shell config.
- Files create/rename/trash/copy/move.
- Terminal scrollback and copy/paste.
- Full-screen screenshot shortcut verified.
- Firefox launches and behaves acceptably.
- A short manual smoke checklist becomes the daily pre-commit validation.

### Milestone 2 - External display and hardware confidence

Goal: The desktop works on the internal display and one external display.

- Multi-output model in compositor.
- Per-output logical geometry, scale, work areas, and layer-shell panels.
- Output hotplug handling.
- Settings display page for output enablement, scale, position, and primary output.
- External-display snap/maximize/launcher behavior.
- Automated or scripted external-display smoke.

### Milestone 3 - Desktop services

Goal: The shell feels like an operating environment, not just panels.

- Network, audio, battery, power, brightness, and clock/calendar top-bar panels.
- Notifications and notification center.
- Clipboard history.
- MIME/default-app/open-with service.
- Shared icon theme provider and user-selectable cursor/icon themes.
- Screenshot UI.
- Lock/logout/reboot/suspend flow.

### Milestone 4 - App completeness

Goal: Native apps cover core workflows.

- Files gets robust operations, search, list view, previews, and mounts.
- Terminal gets profiles, selection polish, Unicode hardening, and mouse/reporting modes.
- Settings covers shell, compositor, display, input, appearance, cursor/icon/font themes, power, network/audio launch points, and about/system info.
- Add Text Editor, Document Viewer, System Monitor, Archive Manager, and Calculator.

### Milestone 5 - Production hardening

Goal: Problems are caught before they are visible.

- Real-app compatibility matrix.
- Screenshot/pixel regression tests for glass/chrome/panels.
- Animation smoothness traces for snap/maximize/unmaximize.
- Resize performance traces for terminal and common apps.
- Crash logs collected by session wrapper.
- Long-running idle and active-use soak tests.
- Package/install docs.

## Near-term engineering recommendations

- Work in vertical pieces: Window Manager first, then Shell, Settings, Files, Terminal, and only then secondary apps.
- Keep manual config acceptable during the Window Manager and Shell phases, but make Settings the active piece before treating Files and Terminal as readiness work.
- Fix minimized-window restore before adding more dock polish. A dock that cannot reliably bring apps back is a daily annoyance.
- Move app launch data out of C++ conditionals and into a registry format. Start simple with built-ins plus `.desktop` ingestion later, then wire icon theme lookup through that registry.
- Add a screenshot-based visual regression harness for the compositor. Recent bugs around glass, shadows, titlebars, panels, and snap flicker are exactly the sort of issues that need image comparison.
- Keep native app scope conservative. Settings, Files, Terminal, Screenshot, Text Editor, Document Viewer, and System Monitor are the high-leverage set.
- Keep XWayland as an explicit decision. Pure Wayland is clean, but daily driving may expose one or two apps that force the conversation.
- Make the session story boring. A reliable `lambda-session` with logs and restart behavior will improve daily use more than another visual feature.

## Practical readiness call

Current state: strong prototype, good for active dogfooding by the developer, not ready as a low-maintenance daily driver.

Most important next step: convert the visually complete surfaces into operational surfaces in the chosen order. That means Window Manager hardening and a minimal session wrapper first, then robust shell app lifecycle, real Settings backends, real file operations, and terminal scrollback/copy/paste before calling the system ready.

Once those are in place, the remaining work becomes a normal desktop polish queue rather than foundational missing infrastructure.
