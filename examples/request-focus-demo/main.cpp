#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <string>

using namespace flux;

namespace pal {
constexpr Color bg = Color::hex(0xF2F2F7);
constexpr Color surface = Color::hex(0xFFFFFF);
constexpr Color border = Color::hex(0xC8C8D0);
constexpr Color borderFocus = Color::hex(0x3A7BD5);
constexpr Color label = Color::hex(0x111118);
constexpr Color sub = Color::hex(0x6E6E80);
constexpr Color accent = Color::hex(0x3A7BD5);
constexpr Color editorBg = Color::hex(0xFAFAFC);
constexpr Color editorFocus = Color::hex(0xE8F0FC);
} // namespace pal

/// Reusable focusable text field — same pattern as the focus demo.
struct FocusField {
  std::string title;

  auto body() const {
    auto text = useState<std::string>({});
    bool focused = useFocus();

    return VStack{
        .spacing = 6.f,
        .hAlign = HorizontalAlignment::Leading,
        .children =
            {
                Text{.text = title,
                     .font = {.size = 13.f, .weight = 600.f},
                     .color = pal::label},
                Rectangle{
                    .frame = {0.f, 0.f, 0.f, 80.f},
                    .cornerRadius = CornerRadius(10.f),
                    .fill = FillStyle::solid(focused ? pal::editorFocus : pal::editorBg),
                    .stroke = StrokeStyle::solid(focused ? pal::borderFocus : pal::border, focused ? 2.f : 1.f),
                    .flexGrow = 1.f,
                    .focusable = true,
                    .onKeyDown =
                        [text](KeyCode k, Modifiers) {
                          if (k == keys::Delete && !(*text).empty()) {
                            std::string s = *text;
                            s.pop_back();
                            text = std::move(s);
                          }
                          if (k == keys::Escape) {
                            text = std::string{};
                          }
                        },
                    .onTextInput =
                        [text](std::string const& chunk) {
                          if (!chunk.empty()) {
                            text = *text + chunk;
                          }
                        },
                },
                Text{.text = (*text).empty() ? "(empty)" : *text,
                     .font = {.size = 13.f},
                     .color = pal::sub,
                     .wrapping = TextWrapping::Wrap},
            },
    };
  }
};

/// Panel wrapping a FocusField; writes the subtree request-focus callable into \p focusBind each build.
struct EditorPanel {
  std::string title;
  State<std::function<void()>> focusBind;

  auto body() const {
    auto requestFocus = useRequestFocus();
    focusBind = requestFocus;

    return ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children =
            {
                Rectangle{
                    .cornerRadius = CornerRadius(12.f),
                    .fill = FillStyle::solid(pal::surface),
                    .stroke = StrokeStyle::solid(pal::border, 1.f),
                },
                VStack{
                    .spacing = 10.f,
                    .padding = 14.f,
                    .children =
                        {
                            FocusField{.title = title},
                        },
                },
            },
    };
  }
};

struct RequestFocusDemo {
  auto body() const {
    auto focusA = useState<std::function<void()>>({});
    auto focusB = useState<std::function<void()>>({});

    auto btn = [](std::string label, std::function<void()> action) -> Element {
      return ZStack{
          .children =
              {
                  Rectangle{
                      .frame = {0.f, 0.f, 0.f, 36.f},
                      .cornerRadius = CornerRadius(8.f),
                      .fill = FillStyle::solid(pal::accent),
                      .onTap = std::move(action),
                      .cursor = Cursor::Hand,
                  },
                  Text{.text = std::move(label),
                       .font = {.size = 14.f, .weight = 600.f},
                       .color = Color::hex(0xFFFFFF),
                       .horizontalAlignment = HorizontalAlignment::Center,
                       .verticalAlignment = VerticalAlignment::Center,
                       .padding = 10.f},
              },
      };
    };

    return ZStack{
        .children =
            {
                Rectangle{.fill = FillStyle::solid(pal::bg)},
                VStack{
                    .spacing = 20.f,
                    .padding = 24.f,
                    .hAlign = HorizontalAlignment::Leading,
                    .children =
                        {
                            Text{.text = "useRequestFocus demo",
                                 .font = {.size = 22.f, .weight = 700.f},
                                 .color = pal::label},
                            Text{
                                .text = "Click the buttons below to focus a field programmatically — without "
                                        "clicking on it.",
                                .font = {.size = 14.f},
                                .color = pal::sub,
                                .wrapping = TextWrapping::Wrap},
                            HStack{
                                .spacing = 16.f,
                                .vAlign = VerticalAlignment::Top,
                                .children =
                                    {
                                        Element{EditorPanel{.title = "Panel A", .focusBind = focusA}}.withFlex(1.f),
                                        Element{EditorPanel{.title = "Panel B", .focusBind = focusB}}.withFlex(1.f),
                                    },
                            },
                            HStack{
                                .spacing = 10.f,
                                .children =
                                    {
                                        btn("Focus A", [focusA] {
                                          if (*focusA) {
                                            (*focusA)();
                                          }
                                        }),
                                        btn("Focus B", [focusB] {
                                          if (*focusB) {
                                            (*focusB)();
                                          }
                                        }),
                                    },
                            },
                            Text{
                                .text = "\"Focus A\" and \"Focus B\" call the requestFocus callable returned by "
                                        "useRequestFocus() inside each panel's body(). The callable finds the first "
                                        "focusable leaf in the panel's subtree and calls setFocus on it. Tab / "
                                        "Shift+Tab still cycle between fields normally.",
                                .font = {.size = 13.f},
                                .color = pal::sub,
                                .wrapping = TextWrapping::Wrap},
                        },
                },
            },
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {560, 500},
      .title = "Flux — useRequestFocus demo",
      .resizable = true,
  });
  w.setView<RequestFocusDemo>();
  return app.exec();
}
