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

#include <string>
#include <vector>

using namespace flux;

namespace {

/// Maps the full UTF-8 buffer to non-overlapping attributed runs (required by \c TextArea).
std::vector<AttributedRun> simpleMarkdownStyler(std::string_view sv, Theme const& theme, Font const& baseFont,
                                                  Color const& baseColor) {
  std::string const text(sv);
  std::uint32_t const n = static_cast<std::uint32_t>(text.size());
  std::vector<AttributedRun> runs;
  if (n == 0) {
    return runs;
  }

  Font headingFont = resolveFont(kFontFromTheme, theme.typeHeading.toFont());
  Font codeFont = resolveFont(kFontFromTheme, theme.typeCode.toFont());
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
          push(p + 1, q, codeFont, theme.colorAccent);
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
      Color hc = theme.colorAccent;
      if (hashes >= 2) {
        hc = theme.colorTextPrimary;
      }
      if (hashes >= 3) {
        hc = theme.colorTextSecondary;
      }
      push(ls, contentEnd, headingFont, hc);
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
      push(ls, j + 2, baseFont, theme.colorTextMuted);
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

  std::uint32_t lineStart = 0;
  for (std::uint32_t i = 0; i < n; ++i) {
    if (text[static_cast<std::size_t>(i)] == '\n') {
      processLine(lineStart, i + 1);
      lineStart = i + 1;
    }
  }
  if (lineStart < n) {
    processLine(lineStart, n);
  }

  return runs;
}

} // namespace

struct MarkdownEditor {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    auto doc = useState(std::string {
        "# Markdown styler\n"
        "## TextArea `.styler`\n"
        "This demo uses a small **attributed-text** callback. Edit this buffer — headings, list lines, "
        "`inline code`, and **bold** update as you type.\n"
        "\n"
        "### Try it\n"
        "- Use `#` / `##` / `###` at line start\n"
        "- Wrap **bold** in double asterisks\n"
        "- Use `backticks` for code-colored spans\n"});

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
                    .height = {.minIntrinsic = 200.f, .maxIntrinsic = 560.f},
                    .styler = [theme, baseFont, baseColor](std::string_view s) {
                      return simpleMarkdownStyler(s, theme, baseFont, baseColor);
                    },
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
