// Demonstrates single-line TextInput: Tab focus, clipboard actions, Return submit.
#include <Flux.hpp>
#include <Flux/Core/Action.hpp>
#include <Flux/Core/Shortcut.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/Theme.hpp>
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
    Theme const& theme = useEnvironment<Theme>();
    auto name = useState(std::string{"Abdurrahman Avcı"});
    auto email = useState(std::string{"abdurrahmanavci@gmail.com"});
    auto notes = useState(std::string{"Try Cmd+A / C / X / V, Tab between fields."});
    auto disabledSample = useState(std::string{"read-only"});

    return VStack{
        .spacing = 16.f,
        .children = children(
                Text{.text = "TextInput demo",
                     .style = theme.typeDisplay,
                     .color = theme.colorTextPrimary},
                HStack{
                    .spacing = 0.f,
                    .children = children(
                            Text{
                                .text = "Tab / Shift+Tab between fields. Edit actions use the window action table.",
                                .style = theme.typeBodySmall,
                                .color = theme.colorTextSecondary,
                                .wrapping = TextWrapping::Wrap,
                            }
                                .flex(1.f)
                        ),
                },
                Text{.text = "Name",
                     .style = theme.typeLabel,
                     .color = theme.colorTextPrimary},
                HStack{
                    .spacing = 0.f,
                    .children = children(
                            TextInput{
                                .value = name,
                                .placeholder = "Your name",
                                .onSubmit = [](std::string const& v) {
                                  std::fprintf(stderr, "[textinput-demo] submit name: %s\n", v.c_str());
                                },
                            }
                                .flex(1.f)
                        ),
                },
                Text{.text = "Email",
                     .style = theme.typeLabel,
                     .color = theme.colorTextPrimary},
                HStack{
                    .spacing = 0.f,
                    .children = children(
                            TextInput{
                                .value = email,
                                .placeholder = "you@example.com",
                                .onChange = [](std::string const&) {},
                                .onSubmit = [](std::string const& v) {
                                  std::fprintf(stderr, "[textinput-demo] submit email: %s\n", v.c_str());
                                },
                            }
                                .flex(1.f)
                        ),
                },
                Text{.text = "Notes",
                     .style = theme.typeLabel,
                     .color = theme.colorTextPrimary},
                HStack{
                    .spacing = 0.f,
                    .children = children(
                            TextInput{
                                .value = notes,
                                .placeholder = "Optional",
                            }
                                .flex(1.f, 1.f, 200.f)
                        ),
                },
                Text{.text = "Disabled",
                     .style = theme.typeLabel,
                     .color = theme.colorTextPrimary},
                HStack{
                    .spacing = 0.f,
                    .children = children(
                            TextInput{.value = disabledSample, .placeholder = "N/A", .disabled = true}
                                .flex(1.f)
                        ),
                }
            ),
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


  w.setView<TextInputForm>();
  return app.exec();
}
