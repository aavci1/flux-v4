// Demonstrates Picker: string / int / enum values, keyboard navigation, disabled state.
#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
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
    auto language = useState(std::string{"en"});
    auto count = useState(3);
    auto priority = useState(Priority::Medium);

    return VStack{
        .spacing = 16.f,
        .padding = 24.f,
        .hAlign = HorizontalAlignment::Leading,
        .children =
            {
                Text{.text = "Picker demo",
                     .font = {.size = 22.f, .weight = 700.f},
                     .color = Color::hex(0x111118)},
                HStack{
                    .spacing = 0.f,
                    .children =
                        {
                            Text{
                                .text = "Tab to the pickers. Return / Space opens; arrows move; Escape dismisses.",
                                .font = {.size = 13.f, .weight = 400.f},
                                .color = Color::hex(0x6E6E80),
                                .wrapping = TextWrapping::Wrap,
                            }
                                .withFlex(1.f),
                        },
                },

                Text{.text = "Language",
                     .font = {.size = 13.f, .weight = 600.f},
                     .color = Color::hex(0x3A3A44)},
                HStack{
                    .spacing = 0.f,
                    .children =
                        {
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
                                .withFlex(1.f),
                        },
                },

                Text{.text = "Count",
                     .font = {.size = 13.f, .weight = 600.f},
                     .color = Color::hex(0x3A3A44)},
                HStack{
                    .spacing = 0.f,
                    .children =
                        {
                            Picker<int>{
                                .value = count,
                                .options = {{1, "1"}, {2, "2"}, {3, "3"}, {5, "5"}, {10, "10"}},
                                .onChange = [](int v) {
                                  std::fprintf(stderr, "[picker-demo] count: %d\n", v);
                                },
                            }
                                .withFlex(1.f),
                        },
                },

                Text{.text = "Priority",
                     .font = {.size = 13.f, .weight = 600.f},
                     .color = Color::hex(0x3A3A44)},
                HStack{
                    .spacing = 0.f,
                    .children =
                        {
                            Picker<Priority>{
                                .value = priority,
                                .options =
                                    {
                                        {Priority::Low, "Low"},
                                        {Priority::Medium, "Medium"},
                                        {Priority::High, "High"},
                                    },
                            }
                                .withFlex(1.f),
                        },
                },

                Text{.text = "Disabled sample",
                     .font = {.size = 13.f, .weight = 600.f},
                     .color = Color::hex(0x3A3A44)},
                HStack{
                    .spacing = 0.f,
                    .children =
                        {
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
                                .withFlex(1.f),
                        },
                },
            },
    };
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
