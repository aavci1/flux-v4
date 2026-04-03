// Demonstrates single-line TextInput: Tab focus, clipboard actions, Return submit.
#include <Flux.hpp>
#include <Flux/Core/Action.hpp>
#include <Flux/Core/Shortcut.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/TextInput.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <cstdio>
#include <string>

using namespace flux;

struct TextInputForm {
  auto body() const {
    auto name = useState(std::string{"Abdurrahman Avcı"});
    auto email = useState(std::string{"abdurrahmanavci@gmail.com"});
    auto notes = useState(std::string{"Try Cmd+A / C / X / V, Tab between fields."});
    auto disabledSample = useState(std::string{"read-only"});

    return VStack{
        .spacing = 16.f,
        .hAlign = HorizontalAlignment::Leading,
        .children =
            {
                Text{.text = "TextInput demo",
                     .font = {.size = 22.f, .weight = 700.f},
                     .color = Color::hex(0x111118)},
                HStack{
                    .spacing = 0.f,
                    .children =
                        {
                            Text{
                                .text = "Tab / Shift+Tab between fields. Edit actions use the window action table.",
                                .font = {.size = 13.f, .weight = 400.f},
                                .color = Color::hex(0x6E6E80),
                                .wrapping = TextWrapping::Wrap,
                            }
                                .flex(1.f),
                        },
                },
                Text{.text = "Name",
                     .font = {.size = 13.f, .weight = 600.f},
                     .color = Color::hex(0x3A3A44)},
                HStack{
                    .spacing = 0.f,
                    .children =
                        {
                            TextInput{
                                .value = name,
                                .placeholder = "Your name",
                                .onSubmit = [](std::string const& v) {
                                  std::fprintf(stderr, "[textinput-demo] submit name: %s\n", v.c_str());
                                },
                            }
                                .flex(1.f),
                        },
                },
                Text{.text = "Email",
                     .font = {.size = 13.f, .weight = 600.f},
                     .color = Color::hex(0x3A3A44)},
                HStack{
                    .spacing = 0.f,
                    .children =
                        {
                            TextInput{
                                .value = email,
                                .placeholder = "you@example.com",
                                .onChange = [](std::string const&) {},
                                .onSubmit = [](std::string const& v) {
                                  std::fprintf(stderr, "[textinput-demo] submit email: %s\n", v.c_str());
                                },
                            }
                                .flex(1.f),
                        },
                },
                Text{.text = "Notes",
                     .font = {.size = 13.f, .weight = 600.f},
                     .color = Color::hex(0x3A3A44)},
                HStack{
                    .spacing = 0.f,
                    .children =
                        {
                            TextInput{
                                .value = notes,
                                .placeholder = "Optional",
                            }
                                .flex(1.f, 1.f, 200.f),
                        },
                },
                Text{.text = "Disabled",
                     .font = {.size = 13.f, .weight = 600.f},
                     .color = Color::hex(0x3A3A44)},
                HStack{
                    .spacing = 0.f,
                    .children =
                        {
                            TextInput{.value = disabledSample, .placeholder = "N/A", .disabled = true}
                                .flex(1.f),
                        },
                },
            },
    }.padding(24.f);
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {520, 560},
      .title = "Flux — TextInput",
      .resizable = true,
  });

  w.registerAction("edit.copy", {.label = "Copy", .shortcut = shortcuts::Copy});
  w.registerAction("edit.cut", {.label = "Cut", .shortcut = shortcuts::Cut});
  w.registerAction("edit.paste", {.label = "Paste", .shortcut = shortcuts::Paste});
  w.registerAction("edit.selectAll", {.label = "Select All", .shortcut = shortcuts::SelectAll});
  w.registerAction("app.quit", {.label = "Quit", .shortcut = shortcuts::Quit, .isEnabled = [] { return true; }});

  w.setView<TextInputForm>();
  return app.exec();
}
