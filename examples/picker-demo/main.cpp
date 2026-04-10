// Demonstrates Picker: string / int / enum values, keyboard navigation, disabled state.
#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Picker.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <cstdio>
#include <string>

using namespace flux;

enum class Priority { Low, Medium, High };

struct TaskForm {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    auto language = useState(std::string{"en"});
    auto count = useState(3);
    auto priority = useState(Priority::Medium);

    return VStack{
        .spacing = 16.f,
        .alignment = Alignment::Start,
        .children = children(
                Text{.text = "Picker demo",
                     .font = theme.fontDisplay,
                     .color = theme.colorTextPrimary},
                HStack{
                    .spacing = 0.f,
                    .children = children(
                            Text{
                                .text = "Tab to the pickers. Return / Space opens; arrows move; Escape dismisses.",
                                .font = theme.fontBodySmall,
                                .color = theme.colorTextSecondary,
                                .wrapping = TextWrapping::Wrap,
                            }.flex(1.f)
                        ),
                },

                Text{.text = "Language",
                     .font = theme.fontLabel,
                     .color = theme.colorTextPrimary},
                HStack{
                    .spacing = 0.f,
                    .children = children(
                            Picker<std::string>{
                                .value = language,
                                .options =
                                    {
                                        {"en", "English"},
                                        {"tr", "Turkish"},
                                        {"de", "German"},
                                        {"fr", "French"},
                                        {"ja", "Japanese"},
                                    },
                                .placeholder = "Select language…",
                                .onChange = [](std::string const& v) {
                                  std::fprintf(stderr, "[picker-demo] language: %s\n", v.c_str());
                                },
                            }
                                .flex(1.f)
                        ),
                },

                Text{.text = "Count",
                     .font = theme.fontLabel,
                     .color = theme.colorTextPrimary},
                HStack{
                    .spacing = 0.f,
                    .children = children(
                            Picker<int>{
                                .value = count,
                                .options = {{1, "1"}, {2, "2"}, {3, "3"}, {5, "5"}, {10, "10"}},
                                .onChange = [](int v) {
                                  std::fprintf(stderr, "[picker-demo] count: %d\n", v);
                                },
                            }
                                .flex(1.f)
                        ),
                },

                Text{.text = "Priority",
                     .font = theme.fontLabel,
                     .color = theme.colorTextPrimary},
                HStack{
                    .spacing = 0.f,
                    .children = children(
                            Picker<Priority>{
                                .value = priority,
                                .options =
                                    {
                                        {Priority::Low, "Low"},
                                        {Priority::Medium, "Medium"},
                                        {Priority::High, "High"},
                                    },
                            }
                                .flex(1.f)
                        ),
                },

                Text{.text = "Disabled sample",
                     .font = theme.fontLabel,
                     .color = theme.colorTextPrimary},
                HStack{
                    .spacing = 0.f,
                    .children = children(
                            Picker<std::string>{
                                .value = language,
                                .options =
                                    {
                                        {"en", "English"},
                                        {"tr", "Turkish"},
                                    },
                                .placeholder = "Unavailable",
                                .disabled = true,
                            }
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
      .title = "Flux — Picker",
      .resizable = true,
  });

  w.setView<TaskForm>();
  return app.exec();
}
