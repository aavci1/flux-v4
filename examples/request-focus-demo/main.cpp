#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
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
    Theme const& theme = useEnvironment<Theme>();
    auto text = useState<std::string>({});
    bool focused = useFocus();

    return VStack{
        .spacing = 6.f,
        .alignment = Alignment::Start,
        .children = children(
                Text{.text = title,
                     .font = theme.fontLabel,
                     .color = pal::label},
                HStack{
                    .spacing = 0.f,
                    .children = children(
                            Rectangle{}
                                .fill(FillStyle::solid(focused ? pal::editorFocus : pal::editorBg))
                                .stroke(StrokeStyle::solid(focused ? pal::borderFocus : pal::border,
                                                           focused ? 2.f : 1.f))
                                .height(80.f)
                                .focusable(true)
                                .onKeyDown(
                                    [text](KeyCode k, Modifiers) {
                                      if (k == keys::Delete && !(*text).empty()) {
                                        std::string s = *text;
                                        s.pop_back();
                                        text = std::move(s);
                                      }
                                      if (k == keys::Escape) {
                                        text = std::string{};
                                      }
                                    })
                                .onTextInput(
                                    [text](std::string const& chunk) {
                                      if (!chunk.empty()) {
                                        text = *text + chunk;
                                      }
                                    })
                                .cornerRadius(10.f)
                                .flex(1.f)
                        ),
                },
                HStack{
                    .spacing = 0.f,
                    .children = children(
                            Text{.text = (*text).empty() ? "(empty)" : *text,
                                 .font = theme.fontBodySmall,
                                 .color = pal::sub,
                                 .wrapping = TextWrapping::Wrap}
                                .flex(1.f)
                        ),
                }
            ),
    };
  }
};

/// Panel wrapping a FocusField; writes the subtree request-focus callable into \p focusFnOut each build.
/// Callers pass a pointer to storage that lives as long as the window (e.g. root \c mutable members).
struct EditorPanel {
  std::string title;
  std::function<void()>* focusFnOut = nullptr;

  auto body() const {
    auto requestFocus = useRequestFocus();
    if (focusFnOut) {
      *focusFnOut = requestFocus;
    }

    return ZStack{
        .horizontalAlignment = Alignment::Start,
        .verticalAlignment = Alignment::Start,
        .children = children(
                Rectangle{}
                    .fill(FillStyle::solid(pal::surface))
                    .stroke(StrokeStyle::solid(pal::border, 1.f))
                    .cornerRadius(12.f),
                VStack{
                    .spacing = 10.f,
                    .children = children(
                            FocusField{.title = title}
                        ),
                }.padding(14.f)
            ),
    };
  }
};

struct RequestFocusDemo {
  /// Stable storage for the latest \c useRequestFocus callables (written each build; not reactive).
  mutable std::function<void()> focusPanelA;
  mutable std::function<void()> focusPanelB;

  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    auto btn = [&](std::string label, std::function<void()> action) -> Element {
      return ZStack{
          .children = children(
                  Rectangle{}
                      .fill(FillStyle::solid(pal::accent))
                      .height(36.f)
                      .cursor(Cursor::Hand)
                      .onTap(std::move(action))
                      .cornerRadius(8.f),
                  Text{.text = std::move(label),
                       .font = theme.fontLabel,
                       .color = theme.colorOnAccent,
                       .horizontalAlignment = HorizontalAlignment::Center,
                       .verticalAlignment = VerticalAlignment::Center,
                   }
                      .padding(10.f)
              ),
      };
    };

    return ZStack{
        .children = children(
                Rectangle{}.fill(FillStyle::solid(pal::bg)),
                VStack{
                    .spacing = 20.f,
                    .alignment = Alignment::Start,
                    .children = children(
                            Text{.text = "useRequestFocus demo",
                                 .font = theme.fontDisplay,
                                 .color = pal::label},
                            HStack{
                                .spacing = 0.f,
                                .children = children(
                                        Text{
                                            .text = "Click the buttons below to focus a field programmatically — "
                                                    "without clicking on it.",
                                            .font = theme.fontBody,
                                            .color = pal::sub,
                                            .wrapping = TextWrapping::Wrap,
                                        }
                                            .flex(1.f)
                                    ),
                            },
                            HStack{
                                .spacing = 16.f,
                                .alignment = Alignment::Start,
                                .children = children(
                                        Element{EditorPanel{.title = "Panel A", .focusFnOut = &focusPanelA}}
                                            .flex(1.f),
                                        Element{EditorPanel{.title = "Panel B", .focusFnOut = &focusPanelB}}
                                            .flex(1.f)
                                    ),
                            },
                            HStack{
                                .spacing = 10.f,
                                .children = children(
                                        btn("Focus A", [this] {
                                          if (focusPanelA) {
                                            focusPanelA();
                                          }
                                        }),
                                        btn("Focus B", [this] {
                                          if (focusPanelB) {
                                            focusPanelB();
                                          }
                                        })
                                    ),
                            },
                            HStack{
                                .spacing = 0.f,
                                .children = children(
                                        Text{
                                            .text = "\"Focus A\" and \"Focus B\" call the requestFocus callable returned by "
                                                    "useRequestFocus() inside each panel's body(). The callable finds the first "
                                                    "focusable leaf in the panel's subtree and calls setFocus on it. Tab / "
                                                    "Shift+Tab still cycle between fields normally.",
                                            .font = theme.fontBodySmall,
                                            .color = pal::sub,
                                            .wrapping = TextWrapping::Wrap,
                                        }
                                            .flex(1.f)
                                    ),
                            }
                        ),
                }.padding(24.f)
            ),
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
