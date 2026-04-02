// Demonstrates Window actions: registerAction, useViewAction, useWindowAction, isActionEnabled.
#include <Flux.hpp>
#include <Flux/Core/Action.hpp>
#include <Flux/Core/Shortcut.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <cstdio>
#include <string>

using namespace flux;

namespace {

Window* gActionsEditorWindow = nullptr;

void saveDemo(std::string const& content) {
  std::fprintf(stderr, "[actions-editor-demo] save (%zu bytes)\n", content.size());
  (void)content;
}

} // namespace

struct TextEditor {
  State<std::string> text;
  State<int> selStart;
  State<int> selEnd;

  auto body() const {
    State<std::string> const text = this->text;
    State<int> const selStart = this->selStart;
    State<int> const selEnd = this->selEnd;

    useViewAction(
        "edit.copy",
        [text, selStart, selEnd] {
          int const a = *selStart;
          int const b = *selEnd;
          int const i0 = std::min(a, b);
          int const i1 = std::max(a, b);
          std::string s =
              (*text).substr(static_cast<std::size_t>(i0), static_cast<std::size_t>(i1 - i0));
          Application::instance().clipboard().writeText(std::move(s));
        },
        [selStart, selEnd] { return *selStart != *selEnd; });

    useViewAction(
        "edit.cut",
        [text, selStart, selEnd] {
          int const a = *selStart;
          int const b = *selEnd;
          int const i0 = std::min(a, b);
          int const i1 = std::max(a, b);
          std::string s =
              (*text).substr(static_cast<std::size_t>(i0), static_cast<std::size_t>(i1 - i0));
          Application::instance().clipboard().writeText(std::move(s));
          std::string t = *text;
          t.erase(static_cast<std::size_t>(i0), static_cast<std::size_t>(i1 - i0));
          text = std::move(t);
          selStart = i0;
          selEnd = i0;
        },
        [selStart, selEnd] { return *selStart != *selEnd; });

    useViewAction(
        "edit.paste",
        [text, selStart, selEnd] {
          if (auto s = Application::instance().clipboard().readText()) {
            int const a = *selStart;
            int const b = *selEnd;
            int const i0 = std::min(a, b);
            int const i1 = std::max(a, b);
            std::string t = *text;
            t.erase(static_cast<std::size_t>(i0), static_cast<std::size_t>(i1 - i0));
            t.insert(static_cast<std::size_t>(i0), *s);
            text = std::move(t);
            int const newPos = i0 + static_cast<int>(s->size());
            selStart = newPos;
            selEnd = newPos;
          }
        },
        [] { return Application::instance().clipboard().hasText(); });

    useViewAction("edit.selectAll", [text, selStart, selEnd] {
      selStart = 0;
      selEnd = static_cast<int>((*text).size());
    });

    bool const focused = useFocus();

    return VStack{
        .spacing = 8.f,
        .hAlign = HorizontalAlignment::Leading,
        .children =
            {
                HStack{
                    .spacing = 0.f,
                    .children =
                        {
                            Rectangle{
                                .fill = FillStyle::solid(focused ? Color::hex(0xE8F0FC) : Color::hex(0xFAFAFC)),
                                .stroke = StrokeStyle::solid(focused ? Color::hex(0x3A7BD5) : Color::hex(0xC8C8D0),
                                                               focused ? 2.f : 1.f),
                            }
                                .height(200.f)
                                .focusable(true)
                                .onKeyDown(
                                    [text, selStart, selEnd](KeyCode k, Modifiers m) {
                                      (void)m;
                                      int a = *selStart;
                                      int b = *selEnd;
                                      int i0 = std::min(a, b);
                                      int i1 = std::max(a, b);
                                      std::string cur = *text;
                                      int const len = static_cast<int>(cur.size());

                                      auto collapse = [&](int pos) {
                                        pos = std::clamp(pos, 0, len);
                                        selStart = pos;
                                        selEnd = pos;
                                      };

                                      if (k == keys::LeftArrow) {
                                        if (i0 < i1) {
                                          collapse(i0);
                                        } else {
                                          collapse(i0 - 1);
                                        }
                                        return;
                                      }
                                      if (k == keys::RightArrow) {
                                        if (i0 < i1) {
                                          collapse(i1);
                                        } else {
                                          collapse(i0 + 1);
                                        }
                                        return;
                                      }
                                      if (k == keys::Delete) {
                                        if (i0 < i1) {
                                          cur.erase(static_cast<std::size_t>(i0), static_cast<std::size_t>(i1 - i0));
                                          text = std::move(cur);
                                          collapse(i0);
                                        } else if (i0 > 0) {
                                          cur.erase(static_cast<std::size_t>(i0 - 1), 1);
                                          text = std::move(cur);
                                          collapse(i0 - 1);
                                        }
                                        return;
                                      }
                                      if (k == keys::ForwardDelete) {
                                        if (i0 < i1) {
                                          cur.erase(static_cast<std::size_t>(i0), static_cast<std::size_t>(i1 - i0));
                                          text = std::move(cur);
                                          collapse(i0);
                                        } else if (i0 < len) {
                                          cur.erase(static_cast<std::size_t>(i0), 1);
                                          text = std::move(cur);
                                          collapse(i0);
                                        }
                                        return;
                                      }
                                      if (k == keys::Return) {
                                        cur.insert(static_cast<std::size_t>(i0), "\n");
                                        text = std::move(cur);
                                        collapse(i0 + 1);
                                      }
                                    })
                                .onTextInput(
                                    [text, selStart, selEnd](std::string const& chunk) {
                                      if (chunk.empty()) {
                                        return;
                                      }
                                      int a = *selStart;
                                      int b = *selEnd;
                                      int i0 = std::min(a, b);
                                      int i1 = std::max(a, b);
                                      std::string cur = *text;
                                      cur.erase(static_cast<std::size_t>(i0), static_cast<std::size_t>(i1 - i0));
                                      cur.insert(static_cast<std::size_t>(i0), chunk);
                                      text = std::move(cur);
                                      int const p = i0 + static_cast<int>(chunk.size());
                                      selStart = p;
                                      selEnd = p;
                                    })
                                .cornerRadius(CornerRadius(10.f))
                                .flex(1.f, 1.f, 120.f),
                        },
                },
                HStack{
                    .spacing = 0.f,
                    .children =
                        {
                            Text{.text = (*text).empty() ? std::string("(empty)") : std::string(*text),
                                 .font = {.size = 14.f, .weight = 400.f},
                                 .color = Color::hex(0x3A3A44),
                                 .wrapping = TextWrapping::Wrap}
                                .flex(1.f),
                        },
                },
            },
    };
  }
};

struct Toolbar {
  State<std::string> text;
  State<int> selStart;
  State<int> selEnd;

  auto body() const {
    State<std::string> const text = this->text;
    State<int> const selStart = this->selStart;
    State<int> const selEnd = this->selEnd;

    useWindowAction("file.save", [text] { saveDemo(*text); });

    useWindowAction("file.new", [text, selStart, selEnd] {
      text = std::string{};
      selStart = 0;
      selEnd = 0;
    });

    useWindowAction("app.quit", [] { Application::instance().quit(); });

    bool const canCopy = gActionsEditorWindow && gActionsEditorWindow->isActionEnabled("edit.copy");
    bool const canPaste = gActionsEditorWindow && gActionsEditorWindow->isActionEnabled("edit.paste");

    auto pill = [](char const* label, bool enabled, std::function<void()> tap) {
      return Text{
                 .text = label,
                 .font = {.size = 13.f, .weight = 600.f},
                 .color = enabled ? Color::hex(0x111118) : Color::hex(0xAAAAAA),
             }
          .onTap(enabled ? std::move(tap) : std::function<void()>{})
          .padding(8.f)
          .background(FillStyle::solid(enabled ? Color::hex(0xECECF0) : Color::hex(0xF5F5F7)))
          .border(StrokeStyle::solid(Color::hex(0xC8C8D0), 1.f))
          .cornerRadius(CornerRadius(6.f));
    };

    return HStack{
        .spacing = 8.f,
        .padding = 8.f,
        .vAlign = VerticalAlignment::Center,
        .children =
            {
                pill("New", true, [text, selStart, selEnd] {
                  text = std::string{};
                  selStart = 0;
                  selEnd = 0;
                }),
                pill("Save", true, [text] { saveDemo(*text); }),
                pill("Copy", canCopy,
                     [text, selStart, selEnd] {
                       int const i0 = std::min(*selStart, *selEnd);
                       int const i1 = std::max(*selStart, *selEnd);
                       std::string s =
                           (*text).substr(static_cast<std::size_t>(i0), static_cast<std::size_t>(i1 - i0));
                       Application::instance().clipboard().writeText(std::move(s));
                     }),
                pill("Paste", canPaste,
                     [text, selStart, selEnd] {
                       if (auto s = Application::instance().clipboard().readText()) {
                         int const i0 = std::min(*selStart, *selEnd);
                         int const i1 = std::max(*selStart, *selEnd);
                         std::string t = *text;
                         t.erase(static_cast<std::size_t>(i0), static_cast<std::size_t>(i1 - i0));
                         t.insert(static_cast<std::size_t>(i0), *s);
                         text = std::move(t);
                         int const p = i0 + static_cast<int>(s->size());
                         selStart = p;
                         selEnd = p;
                       }
                     }),
            },
    };
  }
};

struct EditorRoot {
  auto body() const {
    auto text = useState<std::string>(std::string{
        "Select text and try Cmd+C / Cmd+X / Cmd+V / Cmd+A.\n"
        "Cmd+S saves (stderr), Cmd+N clears, Cmd+Q quits."});
    auto selStart = useState<int>(0);
    auto selEnd = useState<int>(0);

    return ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children =
            {
                Rectangle{.fill = FillStyle::solid(Color::hex(0xF2F2F7))},
                VStack{
                    .spacing = 0.f,
                    .hAlign = HorizontalAlignment::Leading,
                    .children =
                        {
                            Element{Toolbar{.text = text, .selStart = selStart, .selEnd = selEnd}},
                            VStack{
                                .spacing = 12.f,
                                .padding = 24.f,
                                .hAlign = HorizontalAlignment::Leading,
                                .children =
                                    {
                                        Text{.text = "Actions editor demo",
                                             .font = {.size = 22.f, .weight = 700.f},
                                             .color = Color::hex(0x111118)},
                                        HStack{
                                            .spacing = 0.f,
                                            .children =
                                                {
                                                    Text{
                                                        .text = "Focus the editor. Toolbar buttons use the same shared "
                                                                "state; enabled flags follow isActionEnabled.",
                                                        .font = {.size = 13.f, .weight = 400.f},
                                                        .color = Color::hex(0x6E6E80),
                                                        .wrapping = TextWrapping::Wrap,
                                                    }
                                                        .flex(1.f),
                                                },
                                        },
                                        HStack{
                                            .spacing = 0.f,
                                            .children =
                                                {
                                                    Element{TextEditor{.text = text,
                                                                      .selStart = selStart,
                                                                      .selEnd = selEnd}}
                                                        .flex(1.f),
                                                },
                                        }
                                            .flex(1.f),
                                    },
                            },
                        },
                },
            },
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {800, 560},
      .title = "Flux — Actions editor demo",
      .resizable = true,
  });

  w.registerAction("edit.copy", {.label = "Copy", .shortcut = shortcuts::Copy});
  w.registerAction("edit.cut", {.label = "Cut", .shortcut = shortcuts::Cut});
  w.registerAction("edit.paste", {.label = "Paste", .shortcut = shortcuts::Paste});
  w.registerAction("edit.selectAll", {.label = "Select All", .shortcut = shortcuts::SelectAll});
  w.registerAction("file.save", {.label = "Save", .shortcut = shortcuts::Save});
  w.registerAction("file.new", {.label = "New", .shortcut = shortcuts::New});
  w.registerAction("app.quit",
                   {.label = "Quit", .shortcut = shortcuts::Quit, .isEnabled = [] { return true; }});

  gActionsEditorWindow = &w;
  w.setView<EditorRoot>();
  return app.exec();
}
