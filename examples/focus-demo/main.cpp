#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <string>

using namespace flux;

namespace pal {
constexpr Color bg = Color::hex(0xF2F2F7);
constexpr Color border = Color::hex(0xC8C8D0);
constexpr Color borderFocus = Color::hex(0x3A7BD5);
constexpr Color label = Color::hex(0x111118);
constexpr Color sublabel = Color::hex(0x6E6E80);
constexpr Color editorBg = Color::hex(0xFAFAFC);
constexpr Color editorBgFocus = Color::hex(0xE8F0FC);
} // namespace pal

/// One focusable text field: tap to focus, type to append, Delete for backspace, Esc clears.
struct FocusField {
  std::string title;

  auto body() const {
    auto text = useState<std::string>(std::string{});
    bool const focused = useFocus();

    return VStack{
        .spacing = 8.f,
        .hAlign = HorizontalAlignment::Leading,
        .children =
            {
                Text{.text = title,
                     .font = {.size = 14.f, .weight = 600.f},
                     .color = pal::label},
                Rectangle{
                    .frame = {0.f, 0.f, 0.f, 120.f},
                    .cornerRadius = CornerRadius(10.f),
                    .fill = FillStyle::solid(focused ? pal::editorBgFocus : pal::editorBg),
                    .stroke = StrokeStyle::solid(focused ? pal::borderFocus : pal::border, focused ? 2.f : 1.f),
                    .flexGrow = 1.f,
                    .minSize = 80.f,
                    .focusable = true,
                    .onKeyDown =
                        [text](KeyCode k, Modifiers m) {
                          std::string const& cur = *text;
                          if (k == keys::Delete && !cur.empty()) {
                            std::string s = cur;
                            s.pop_back();
                            text = std::move(s);
                            return;
                          }
                          if (k == keys::Escape) {
                            text = std::string{};
                            return;
                          }
                          if (k == keys::S && any(m & Modifiers::Meta)) {
                            text = cur + " [saved]";
                          }
                        },
                    .onTextInput =
                        [text](std::string const& chunk) {
                          if (!chunk.empty()) {
                            text = *text + chunk;
                          }
                        },
                },
                Text{.text = (*text).empty() ? std::string("(empty)") : std::string(*text),
                     .font = {.size = 13.f, .weight = 400.f},
                     .color = pal::sublabel,
                     .wrapping = TextWrapping::Wrap,
                     .frame = {0.f, 0.f, 0.f, 0.f}},
            },
    };
  }
};

struct FocusDemoRoot {
  auto body() const {
    return ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children =
            {
                Rectangle{.fill = FillStyle::solid(pal::bg)},
                VStack{
                    .spacing = 20.f,
                    .padding = 24.f,
                    .hAlign = HorizontalAlignment::Leading,
                    .children =
                        {
                            Text{.text = "Focus & keyboard",
                                 .font = {.size = 26.f, .weight = 700.f},
                                 .color = pal::label},
                            Text{
                                .text = "Click a field to focus. Type text; Delete removes a character; Esc "
                                        "clears. Cmd+S appends a demo tag. Switch fields by clicking the other "
                                        "panel. Focus ring uses useFocus().",
                                .font = {.size = 14.f, .weight = 400.f},
                                .color = pal::sublabel,
                                .wrapping = TextWrapping::Wrap,
                                .frame = {0.f, 0.f, 0.f, 0.f}},
                            HStack{
                                .spacing = 16.f,
                                .vAlign = VerticalAlignment::Top,
                                .children =
                                    {
                                        Element{FocusField{.title = "Field A — notes"}}.withFlex(1.f),
                                        Element{FocusField{.title = "Field B — scratch"}}.withFlex(1.f),
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
      .size = {560, 420},
      .title = "Flux — Focus & keyboard demo",
      .resizable = true,
  });
  w.setView<FocusDemoRoot>();
  return app.exec();
}
