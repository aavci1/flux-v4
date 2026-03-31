// Multiline TextArea: Escape clears notes (onEscape).

#include <Flux.hpp>
#include <Flux/Core/Action.hpp>
#include <Flux/Core/Shortcut.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/TextArea.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <cstdio>
#include <string>

using namespace flux;

struct NotesEditor {
  auto body() const {
    auto text = useState(std::string{});

    return VStack{
        .spacing = 8.f,
        .padding = 16.f,
        .children =
            {
                Text{.text = "Notes",
                     .font = {.size = 13.f, .weight = 600.f},
                     .color = Color::hex(0x111118)},
                TextArea{
                    .value = text,
                    .placeholder = "Start typing…",
                    .height = {.minIntrinsic = 120.f, .maxIntrinsic = 400.f},
                    .onChange =
                        [](std::string const& v) {
                          std::fprintf(stderr, "notes length=%zu\n", v.size());
                        },
                    .onEscape = [text](std::string const&) { text = ""; },
                },
            },
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {480, 560},
      .title = "Flux — TextArea",
      .resizable = true,
  });

  w.registerAction("edit.copy", {.label = "Copy", .shortcut = shortcuts::Copy});
  w.registerAction("edit.cut", {.label = "Cut", .shortcut = shortcuts::Cut});
  w.registerAction("edit.paste", {.label = "Paste", .shortcut = shortcuts::Paste});
  w.registerAction("edit.selectAll", {.label = "Select All", .shortcut = shortcuts::SelectAll});
  w.registerAction("app.quit", {.label = "Quit", .shortcut = shortcuts::Quit, .isEnabled = [] { return true; }});

  w.setView<NotesEditor>();
  return app.exec();
}
