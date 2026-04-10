// TextArea `.styler` demo: lightweight markdown-inspired highlighting (headings, bold, `code`).

#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Graphics/AttributedString.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/TextArea.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace flux;

namespace {


enum class Elements {
  h1,
  h2,
  h3,
  body,
  code
};

/// Maps the full UTF-8 buffer to non-overlapping attributed runs (required by \c TextArea).
AttributedString markdownFormatter(std::string_view sv) {
  std::string const text(sv);
  std::uint32_t const n = static_cast<std::uint32_t>(text.size());
  std::vector<AttributedRun> runs;
  if (n == 0) {
    return AttributedString { .utf8 = std::string(sv), .runs = runs };
  }

  auto baseFont = Font {.family = "Menlo", .size = 14.f, .weight = 400.f};
  auto fonts = std::map<Elements, Font> {};
  fonts[Elements::h1] = Font {.size = baseFont.size * 2.0f, .weight = baseFont.weight * 1.25f};
  fonts[Elements::h2] = Font {.size = baseFont.size * 1.6f, .weight = baseFont.weight * 1.25f};
  fonts[Elements::h3] = Font {.size = baseFont.size * 1.3f, .weight = baseFont.weight * 1.25f};
  fonts[Elements::body] = baseFont;
  fonts[Elements::code] = baseFont;

  auto baseColor = Color::hex(0xC2C2C2);
  auto colors = std::map<Elements, Color> {};
  colors[Elements::h1] = baseColor;
  colors[Elements::h2] = baseColor;
  colors[Elements::h3] = baseColor;
  colors[Elements::body] = baseColor;
  colors[Elements::code] = baseColor;

  Font headingFont = resolveFont(kFontFromTheme, fonts[Elements::h1]);
  Font codeFont = resolveFont(kFontFromTheme, fonts[Elements::code]);
  Font boldFont = baseFont;
  boldFont.weight = (boldFont.weight > 0.f ? boldFont.weight : 400.f) + 250.f;
  if (boldFont.weight > 900.f) {
    boldFont.weight = 900.f;
  }

  auto push = [&](std::uint32_t a, std::uint32_t b, Font const& f, Color c) {
    if (a < b) {
      runs.push_back(AttributedRun {a, b, f, c});
    }
    };

  auto parseInline = [&](std::uint32_t lineStart, std::uint32_t lineEnd) {
    std::uint32_t p = lineStart;
    while (p < lineEnd) {
      if (p + 1 < lineEnd && text[static_cast<std::size_t>(p)] == '*' &&
        text[static_cast<std::size_t>(p + 1)] == '*') {
        std::uint32_t q = p + 2;
        while (q + 1 < lineEnd) {
          if (text[static_cast<std::size_t>(q)] == '*' && text[static_cast<std::size_t>(q + 1)] == '*') {
            break;
          }
          ++q;
        }
        if (q + 1 < lineEnd && text[q] == '*' && text[q + 1] == '*') {
          push(p, p + 2, baseFont, baseColor);
          push(p + 2, q, boldFont, baseColor);
          push(q, q + 2, baseFont, baseColor);
          p = q + 2;
          continue;
        }
      }
      if (text[static_cast<std::size_t>(p)] == '`') {
        std::uint32_t q = p + 1;
        while (q < lineEnd && text[static_cast<std::size_t>(q)] != '`') {
          ++q;
        }
        if (q < lineEnd) {
          push(p, p + 1, baseFont, baseColor);
          push(p + 1, q, codeFont, colors[Elements::code]);
          push(q, q + 1, baseFont, baseColor);
          p = q + 1;
          continue;
        }
      }
      std::uint32_t const start = p;
      ++p;
      while (p < lineEnd) {
        char const ch = text[static_cast<std::size_t>(p)];
        if (ch == '`' || (p + 1 < lineEnd && ch == '*' && text[static_cast<std::size_t>(p + 1)] == '*')) {
          break;
        }
        ++p;
      }
      push(start, p, baseFont, baseColor);
    }
    };

  auto processLine = [&](std::uint32_t ls, std::uint32_t le) {
    if (ls >= le) {
      return;
    }
    std::uint32_t contentEnd = le;
    bool const endsNl = contentEnd > ls && text[static_cast<std::size_t>(contentEnd - 1)] == '\n';
    if (endsNl) {
      --contentEnd;
    }

    if (contentEnd <= ls) {
      push(ls, le, baseFont, baseColor);
      return;
    }

    std::uint32_t i = ls;
    int hashes = 0;
    while (i < contentEnd && hashes < 3 && text[static_cast<std::size_t>(i)] == '#') {
      ++hashes;
      ++i;
    }
    if (hashes > 0 && i < contentEnd && text[static_cast<std::size_t>(i)] == ' ') {
      ++i;
      Font f = fonts[Elements::h1];
      Color c = colors[Elements::h1];
      if (hashes >= 2) {
        f = fonts[Elements::h2];
        c = colors[Elements::h2];
      }
      if (hashes >= 3) {
        f = fonts[Elements::h3];
        c = colors[Elements::h3];
      }
      push(ls, contentEnd, f, c);
      if (endsNl) {
        push(contentEnd, le, baseFont, baseColor);
      }
      return;
    }

    std::uint32_t j = ls;
    while (j < contentEnd && text[static_cast<std::size_t>(j)] == ' ') {
      ++j;
    }
    if (j < contentEnd && text[static_cast<std::size_t>(j)] == '-' && j + 1 < contentEnd &&
      text[static_cast<std::size_t>(j + 1)] == ' ') {
      push(ls, j + 2, baseFont, baseColor);
      parseInline(j + 2, contentEnd);
      if (endsNl) {
        push(contentEnd, le, baseFont, baseColor);
      }
      return;
    }

    parseInline(ls, contentEnd);
    if (endsNl) {
      push(contentEnd, le, baseFont, baseColor);
    }
    };

  std::uint32_t ls = 0;
  while (ls < n) {
    std::uint32_t le = ls;
    while (le < n) {
      if (text[static_cast<std::size_t>(le)] == '\n') {
        break;
      }
      le++;
    }
    processLine(ls, le + 1);
    ls = le + 1;
  }

  return AttributedString { 
    .utf8 = std::string(sv), 
    .runs = runs
  };
}

} // namespace

struct MarkdownEditor {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    auto doc = useState(std::string {
R"(# TextInput / TextArea / Kernel / Behavior reimplementation

## Constraints from your instructions

- **Do not read or port from** [`src/UI/Views/TextInput.cpp`](src/UI/Views/TextInput.cpp) or [`src/UI/Views/TextArea.cpp`](src/UI/Views/TextArea.cpp); treat them as deleted and implement from the specs only.
- **Backwards compatibility**: only what the specs require; migrate in-tree callers of the new `TextInput::Style` layout.

## API alignment notes (spec vs repo)

| Spec | Repo reality |
|------|----------------|
| `useTextSystem()` | Use [`Application::instance().textSystem()`](include/Flux/Core/Application.hpp) (same as current views). Optionally add a one-liner `useTextSystem()` in [`Hooks.hpp`](include/Flux/UI/Hooks.hpp) if you want the spec’s ergonomics. |
| `handleKey(KeyEvent const&)` | Flux uses [`onKeyDown(KeyCode, Modifiers)`](include/Flux/UI/Element.hpp). Define a small `KeyEvent { KeyCode key; Modifiers modifiers; }` (or implement `handleKey(KeyCode, Modifiers)`) in [`TextEditBehavior.hpp`](include/Flux/UI/Views/TextEditBehavior.hpp) and forward from the view. |
| `Clipboard::getText` / `setText` | Actual API: [`Clipboard::readText`](include/Flux/Core/Clipboard.hpp) / `writeText`; access via `Application::instance().clipboard()`. |
| `LineMetrics` in §4 | Algorithms in §5 reference `line.ctLineIndex`. **Add `std::uint32_t ctLineIndex`** (copy from each [`TextLayout::LineRange`](include/Flux/Graphics/TextLayout.hpp)) so `caretXForByte` / `caretByteAtX` can filter runs without ambiguity. |

## Architecture

```mermaid
flowchart LR
  subgraph components [Views]
    TI[TextInput]
    TA[TextArea]
  end
  subgraph behavior [Stateful layer]
    UTEB[useTextEditBehavior]
    TEB[TextEditBehavior]
  end
  subgraph kernel [Pure helpers]
    TEK[TextEditKernel detail]
  end
  subgraph layout [Layout]
    TL[TextLayout from TextSystem]
  end
  TI --> UTEB
  TA --> UTEB
  UTEB --> TEB
  TI --> TL
  TA --> TL
  TI --> TEK
  TA --> TEK
  TEB --> TEK
```

## Phase 1 — `TextEditKernel`

1. **Replace** [`include/Flux/UI/Views/TextEditKernel.hpp`](include/Flux/UI/Views/TextEditKernel.hpp) with the reduced API: UTF-8 helpers (unchanged list), `shouldCoalesceInsert`, `LineMetrics` (+ `ctLineIndex`), `buildLineMetrics(TextLayout const&)`, `lineIndexForByte` (no `buf` arg), `caretXForByte`, `caretByteAtX(..., std::string const& buf)`, `orderedSelection`. Remove `TextSystem`, `replaceSelection`, blink types, and measure-based caret APIs.
2. **Rewrite** [`src/UI/Views/TextEditKernel.cpp`](src/UI/Views/TextEditKernel.cpp) from the spec’s algorithms:
   - `buildLineMetrics`: iterate [`layout.lines`](include/Flux/Graphics/TextLayout.hpp); for each line, `lineMinX` = min `PlacedRun::origin.x` among runs with matching `ctLineIndex`; fill `byteStart`/`byteEnd`/`top`/`bottom`/`baseline` from `LineRange`.
   - `caretXForByte` / `caretByteAtX`: walk matching runs sorted by `origin.x`; use [`TextRun::positions`](include/Flux/Graphics/TextRun.hpp) / `width` for intra-run math; UTF-8 clamp via existing helpers + `buf`.
3. **Delete** all `TextSystem::measure` usage from this translation unit (per spec §8).

## Phase 2 — `TextEditBehavior`

1. **Add** [`include/Flux/UI/Views/TextEditBehavior.hpp`](include/Flux/UI/Views/TextEditBehavior.hpp): `TextEditBehaviorOptions` (include `verticalResolver`, `std::function<int(int,int)>`; capture-on-first-call + update callbacks/`maxLength`/`acceptsTab` on later builds; debug-assert if `multiline` changes), `TextEditBehavior` class per spec §3, and `useTextEditBehavior(State<std::string>, options)`.
2. **Add** [`src/UI/Views/TextEditBehavior.cpp`](src/UI/Views/TextEditBehavior.cpp):
   - **State**: `Signal<std::string>*` from `State<std::string>`, caret/anchor `int`s, `focused`/`disabled`, undo/redo deques (`UndoEntry` per §3.2), redo clear on mutation, coalescing with `shouldCoalesceInsert` + time window.
   - **Blink**: Move/refactor logic now in [`CaretBlinkTimerSlot`](src/UI/Views/TextEditKernel.cpp) / global `AnimationClock` subscription into private members of `TextEditBehavior` (subscribe when `focused && !disabled`, unsubscribe on blur/destruct).
   - **Mutations**: Internal `insertText` / `deleteRange` replacing old `replaceSelection` (same value/caret/anchor/`onChange`/max-length rules from current kernel behavior, now calling `utf8TruncateToChars` where needed).
   - **Keyboard**: Map [`keys::*`](include/Flux/Core/KeyCodes.hpp) + [`Modifiers`](include/Flux/Core/Types.hpp) (Cmd = `Meta` on macOS) to navigation, selection, clipboard, undo, Enter/Tab/Escape per §3.1; for Up/Down when `multiline`, call `verticalResolver` if set.
   - **Clipboard**: `Application::instance().clipboard()`.
   - **`consumeEnsureCaretVisibleRequest`**: set after mutations that should trigger scroll; components read once per frame/build.
3. **CMake**: Add `src/UI/Views/TextEditBehavior.cpp` beside existing [`TextEditKernel.cpp` line](CMakeLists.txt).

## Phase 3 — `TextInput`

1. **Update** [`include/Flux/UI/Views/TextInput.hpp`](include/Flux/UI/Views/TextInput.hpp): nested `Style` with `Style::plain()`, move chrome fields under `style`, add `styler` and `validationColor`, keep `value` / `placeholder` / `onChange` / `onSubmit` / `maxLength` / `disabled`.
2. **Rewrite** [`src/UI/Views/TextInput.cpp`](src/UI/Views/TextInput.cpp) (new file content only): `resolveStyle` using [`InputFieldChromeSpec`](include/Flux/UI/InputFieldChrome.hpp) + theme (same tokens as today’s flat fields); `useTextEditBehavior` with `multiline=false`, `submitsOnEnter=true`, `acceptsTab=false`; `ts.layout(..., NoWrap, width)` inside measure/render path where final width is known; `buildLineMetrics` → single `LineMetrics`; horizontal scroll via `State<int> scrollStartByte` + `consumeEnsureCaretVisibleRequest`; custom-draw selection/caret/text using `caretXForByte` / `caretByteAtX` with scroll correction on hit-testing; wire focus/pointer/key/text handlers to behavior.
3. **Migration**: Grep for `TextInput` initializers using removed top-level fields; convert to `.style = { ... }`. Current grep shows most sites only set `value` / `placeholder` / handlers — **likely zero or few style migrations**.

## Phase 4 — `TextArea`

1. **Update** [`include/Flux/UI/Views/TextArea.hpp`](include/Flux/UI/Views/TextArea.hpp): add optional `styler` only (per spec).
2. **Rewrite** [`src/UI/Views/TextArea.cpp`](src/UI/Views/TextArea.cpp): `useTextEditBehavior` with `multiline=true`, `acceptsTab=true`, `submitsOnEnter=false`, `onEscape`, `verticalResolver` lambda calling a `resolveVerticalMove` helper using **captured** `TextLayout` + `lineMetrics` from the current build; `StylerCache` memoization when `styler` is set; `buildLineMetrics` for all lines; vertical scroll `State<float> scrollY` + wheel forwarding; multi-line selection loop per spec §2.4–2.5.
3. Keep existing `TextArea::Style` and `resolveStyle` **behavior** (conceptually) — re-express in the new file without copying the old implementation verbatim (read only [`TextArea.hpp`](include/Flux/UI/Views/TextArea.hpp) + shared chrome helpers as needed).

## Phase 5 — Tests and docs

1. **Unit tests** (extend [`CMakeLists.txt`](CMakeLists.txt) `flux_tests` when `FLUX_BUILD_TESTS`): new files e.g. `tests/TextEditKernelTests.cpp` and `tests/TextEditBehaviorTests.cpp` covering items in spec §8–9 (UTF-8, `buildLineMetrics` invariants, caret monotonicity / round-trip on ASCII, `shouldCoalesceInsert`, undo coalescing, max length, clipboard round-trip where feasible headlessly).
2. **Docs**: Update [`docs/TextArea-spec.md`](docs/TextArea-spec.md) to reference the new kernel API (`caretXForByte` / `buildLineMetrics(TextLayout)`).
3. **Comments**: Fix stale references (e.g. [`src/UI/InputFieldLayout.cpp`](src/UI/InputFieldLayout.cpp) “Keep in sync with TextInput.cpp”).
4. **“Visual tests” from spec**: There is **no** automated screenshot harness in-repo. Practical approach: rely on **manual** checks via [`examples/textinput-demo`](examples/textinput-demo/main.cpp) and [`examples/textarea-demo`](examples/textarea-demo/main.cpp), and optional **new** example snippets for styler/selection if desired later.

## In-tree consumers to touch

- Examples: [`examples/textinput-demo/main.cpp`](examples/textinput-demo/main.cpp), [`examples/llm-studio/ModelBrowser.hpp`](examples/llm-studio/ModelBrowser.hpp) — only if `TextInput` initializer breaks compile.
- Any file including [`TextEditKernel.hpp`](include/Flux/UI/Views/TextEditKernel.hpp) for removed symbols (today: only kernel cpp + the two view cpps being rewritten).

## Risk / order

Ship **kernel → behavior → views** so each layer compiles before the next. `TextEditBehavior` is the largest piece (keyboard matrix + undo + blink + clipboard).
)"
      });

    Font const baseFont = resolveFont(kFontFromTheme, theme.typeBody.toFont());
    Color const baseColor = theme.colorTextPrimary;

    return VStack {
        .spacing = 12.f,
        .children =
            children(
                Text {
                    .text = "Markdown styler (TextArea)",
                    .style = theme.typeDisplay,
                    .color = theme.colorTextPrimary,
                },
                Text {
                    .text = "The `.styler` field receives the full document and returns AttributedRun ranges.",
                    .style = theme.typeBody,
                    .color = theme.colorTextSecondary,
                    .wrapping = TextWrapping::Wrap,
                },
                TextArea {
                    .value = doc,
                    .style = {
                      .backgroundColor = Color::hex(0x1F1F1F),
                    },
                    .placeholder = "Write markdown-like text…",
                    .formatter = markdownFormatter,
                }.flex(1.f, 1.f, 0.f)),
    }
    .padding(20.f)
      .flex(1.f, 1.f, 0.f);
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {520, 620},
      .title = "Flux — Markdown styler",
      .resizable = true,
    });

  w.setView<MarkdownEditor>();
  return app.exec();
}
