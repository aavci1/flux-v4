# Lambda Desktop readiness index

**Date:** 2026-05-25
**Status:** Source-of-truth index for the current daily-driver readiness plan.
**Scope:** Lambda desktop readiness across `lambda-window-manager`, `lambda-shell`, `lambda-settings`, `lambda-files`, `lambda-terminal`, shared desktop services, and future core apps.

## Purpose

This document is the main index for the Lambda desktop readiness effort. It defines the implementation order, ownership boundaries, detailed spec links, and the unspecced items that still need product/engineering specs.

The detailed readiness specs are authoritative for actual requirements, acceptance criteria, test plans, and implementation order. This file should stay small enough to answer:

- What are we building first?
- Which spec owns which behavior?
- Which pieces are already specced?
- Which pieces still need specs?

## Current Product Decisions

- The current implementation order is: Window Manager, Shell, Settings, Files, Terminal, then remaining desktop services and apps.
- The first daily-driver target is pure Wayland. XWayland remains a later explicit product decision.
- Sessions are started and ended manually for now. Session automation, greeter/login, logout, lock, suspend/reboot UI, and crash-restart policy are intentionally outside the current detailed specs.
- New log collection and log viewer work are intentionally outside the current detailed specs.
- Browser, mail, calendar, and media can be mature external Wayland apps for the first daily-driver target unless we explicitly decide Lambda should own them.
- Settings edits component-owned config. It should not become a second source of truth for Window Manager, Shell, Files, or Terminal preferences.

## Detailed Specs

| Order | Area | Detailed Spec | Owns |
| --- | --- | --- | --- |
| 1 | Window Manager | [lambda-window-manager-readiness-spec.md](lambda-window-manager-readiness-spec.md) | KMS/Wayland compositor, window management, output/scale, input routing, glass/chrome/shadows, screenshots and compositor-drawn screenshot UI, protocol validation |
| 2 | Shell | [lambda-shell-readiness-spec.md](lambda-shell-readiness-spec.md) | Top bar, dock, command launcher, app registry presentation, icon theme use, notifications, quick settings/status controls, clipboard history, Shell config |
| 3 | Settings | [lambda-settings-readiness-spec.md](lambda-settings-readiness-spec.md) | GUI editor for Window Manager and Shell config, appearance, display, keyboard, dock/panel, system/about status, safe config writes |
| 4 | Files | [lambda-files-readiness-spec.md](lambda-files-readiness-spec.md) | Safe local file management, trash-first delete, selection, clipboard/DnD, operations, open-with, file icons, watchers |
| 5 | Terminal | [lambda-terminal-readiness-spec.md](lambda-terminal-readiness-spec.md) | Pty/libvterm terminal, scrollback, selection, clipboard, keys, mouse reporting, Unicode, colors, preferences, performance |

## Ownership Boundaries

These decisions are intentionally captured here so related specs do not drift.

- Screenshot capture and screenshot UI belong to the Window Manager. The compositor may draw the full-output/window/region selection UI directly, just like it already draws snap indicators and window chrome.
- Notifications belong to the Shell. A helper service may exist internally, but notification banners, notification center, history, grouping, and do-not-disturb are Shell-owned.
- Quick settings and status controls belong to the Shell. Settings owns deeper configuration pages; Shell owns the repeated status/control surface.
- Clipboard history belongs to the Shell at the user-facing level. The Window Manager only owns protocol plumbing when needed.
- App registry and icon theme lookup should become shared desktop infrastructure. Shell is the first major owner, Window Manager consumes it for launch validation/execution, and Files/Settings consume it for open-with/icons/preferences.
- Open-with, MIME, and default-app behavior should become shared desktop infrastructure. Files is the first major consumer; Settings later edits defaults.
- Cursor theme is Window Manager config. Icon theme is Shell/shared desktop config. Settings edits both.
- Window/background/chrome/glass rendering is Window Manager-owned on Linux. Flux/macOS platform behavior remains backend-specific.

## Readiness Status

The project is in active dogfooding state. The core processes and apps exist and are useful, but the system is not daily-driver complete until the detailed specs above are implemented and validated.

Use each detailed spec's `Done checklist` as the readiness gate for that component. This index should not duplicate those checklists.

Current spec coverage:

- Window Manager: detailed spec exists.
- Shell: detailed spec exists.
- Settings: detailed spec exists.
- Files: detailed spec exists.
- Terminal: detailed spec exists.

## Validation Model

Each detailed spec owns its own validation checklist. The general rule is:

- Unit/model tests for deterministic behavior.
- Manual hardware/app smoke tests for compositor, shell, and real Wayland client behavior.
- Real-app validation with Lambda apps plus mature Wayland clients such as Firefox, GTK apps, Qt apps, and `foot`.
- Visual/glass/animation behavior should eventually gain screenshot or render regression coverage, but only where the detailed specs call for it.

## Unspecified Items

These are the remaining areas that still need detailed specs.

### Shared Desktop Services

A shared services architecture spec is still needed so Shell, Settings, Files, and Terminal do not each invent separate backends.

Likely scope:

- App permissions and portal direction for untrusted apps.
- Screenshot/screencast/file chooser portals when exposed to untrusted clients.
- Secrets/keyring.
- MIME/default-app/open-with ownership beyond the first Files implementation.
- Shared app registry service boundaries.
- Shared icon and MIME metadata ownership.
- Shared provider model for status/system information used by Shell and Settings.

Session automation and new log infrastructure are not part of this item unless we explicitly reintroduce them.

### Preliminary Text Editor

Needed because a basic desktop should be able to edit plain text without relying on a terminal editor.

Likely scope:

- Plain-text open/edit/save/save-as.
- New file flow.
- Encoding and newline handling.
- Dirty state.
- Search.
- Clipboard.
- Basic keyboard shortcuts.
- Files/open-with integration.

### Document/PDF Viewer

Needed because a basic desktop should be able to open common documents from Files.

Likely scope:

- PDF rendering.
- Page navigation.
- Zoom and fit modes.
- Search.
- Thumbnail/sidebar navigation.
- Files/open-with integration.
- Print/export can be deferred unless needed for the first usable version.

### Optional Later Apps

These should be specced only when we decide Lambda should own them instead of relying on mature external apps:

- Archive manager.
- Image viewer.
- Media player.
- System monitor.
- Browser.
- Mail.
- Calendar.
- Calculator.

## How To Update This Index

- Add a new detailed spec link when a new area is specced.
- Move an item out of `Unspecified Items` only after its detailed spec exists.
- Keep requirements and acceptance criteria in the detailed specs, not here.
- Update ownership boundaries here when a cross-component decision changes.
