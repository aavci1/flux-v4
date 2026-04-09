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
std::vector<AttributedRun> markdownStyler(std::string_view sv) {
  std::string const text(sv);
  std::uint32_t const n = static_cast<std::uint32_t>(text.size());
  std::vector<AttributedRun> runs;
  if (n == 0) {
    return runs;
  }

  auto baseFont = Font { .family = "Menlo", .size = 14.f, .weight = 400.f };
  auto fonts = std::map<Elements, Font> {};
  fonts[Elements::h1] = Font { .size = baseFont.size * 2.0f, .weight = baseFont.weight * 1.25f };
  fonts[Elements::h2] = Font { .size = baseFont.size * 1.6f, .weight = baseFont.weight * 1.25f };
  fonts[Elements::h3] = Font { .size = baseFont.size * 1.3f, .weight = baseFont.weight * 1.25f };
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
      runs.push_back(AttributedRun{a, b, f, c});
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

  return runs;
}

} // namespace

struct MarkdownEditor {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    auto doc = useState(std::string {
R"(# Header 1

## Header 2

### Header 3

This is **bold**.

This is `inline-code`.
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
                    .placeholder = "Write markdown-like text…",
                    .style = {
                      .backgroundColor = Color::hex(0x1F1F1F),
                    },
                    .styler = markdownStyler,
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

  w.registerAction("edit.copy", {.label = "Copy", .shortcut = shortcuts::Copy});
  w.registerAction("edit.cut", {.label = "Cut", .shortcut = shortcuts::Cut});
  w.registerAction("edit.paste", {.label = "Paste", .shortcut = shortcuts::Paste});
  w.registerAction("edit.selectAll", {.label = "Select All", .shortcut = shortcuts::SelectAll});
  w.registerAction("app.quit", {.label = "Quit", .shortcut = shortcuts::Quit, .isEnabled = [] { return true; }});

  w.setView<MarkdownEditor>();
  return app.exec();
}
