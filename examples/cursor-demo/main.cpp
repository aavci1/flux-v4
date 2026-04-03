#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <string>

using namespace flux;

namespace pal {
constexpr Color bg = Color::hex(0xF0F0F5);
constexpr Color label = Color::hex(0x141418);
constexpr Color sub = Color::hex(0x5C5C6A);
constexpr Color chipDefault = Color::hex(0xE8E8EE);
constexpr Color chipHand = Color::hex(0xD8E8FF);
constexpr Color chipResize = Color::hex(0xE4F5E8);
constexpr Color chipWarn = Color::hex(0xFFE8E0);
constexpr Color chipAccent = Color::hex(0x3A7BD5);
} // namespace pal

namespace {

Element cursorSwatchRow(std::string name, Cursor cursor, Color chip, Theme const& theme) {
  return HStack{
      .spacing = 0.f,
      .children = children(
              HStack{
                    .spacing = 12.f,
                    .vAlign = VerticalAlignment::Center,
                    .children = children(
                            Text{.text = std::move(name),
                                 .style = theme.typeLabel,
                                 .color = pal::label,
                             }
                                .size(152.f, 0.f),
                            Rectangle{
                                .fill = FillStyle::solid(chip),
                                .stroke = StrokeStyle::solid(Color::hex(0xC8C8D0), 1.f),
                            }
                                .height(44.f)
                                .cursor(cursor)
                                .cornerRadius(CornerRadius(8.f))
                                .flex(1.f)
                        ),
                }
                  .flex(1.f)
          ),
  };
}

} // namespace

/// Hover each row to see the platform cursor; drag the bottom strip to verify the cursor stays locked.
struct CursorDemo {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    return ScrollView{
        .axis = ScrollAxis::Vertical,
        .children = children(
                VStack{
                    .spacing = 0.f,
                    .children = children(
                            VStack{
                                .spacing = 6.f,
                                .hAlign = HorizontalAlignment::Leading,
                                .children = children(
                                        Text{.text = "Cursor shapes",
                                             .style = theme.typeDisplay,
                                             .color = pal::label},
                                        HStack{
                                            .spacing = 0.f,
                                            .children = children(
                                                    Text{
                                                            .text = "Move the pointer over each swatch. Drag the resize strip — the "
                                                                    "cursor stays locked to that node during the drag.",
                                                            .style = theme.typeBody,
                                                            .color = pal::sub,
                                                            .wrapping = TextWrapping::Wrap,
                                                        }
                                                        .flex(1.f)
                                                ),
                                        }
                                    ),
                            }.padding(20.f),
                            ScrollView{
                                .axis = ScrollAxis::Vertical,
                                .children = children(
                                        VStack{
                                            .spacing = 10.f,
                                            .children = children(
                                                    cursorSwatchRow("Default", Cursor::Arrow,
                                                                    pal::chipDefault, theme),
                                                    cursorSwatchRow("Hand", Cursor::Hand, pal::chipHand, theme),
                                                    cursorSwatchRow("ResizeEW", Cursor::ResizeEW,
                                                                    pal::chipResize, theme),
                                                    cursorSwatchRow("ResizeNS", Cursor::ResizeNS,
                                                                    pal::chipResize, theme),
                                                    cursorSwatchRow("ResizeNESW", Cursor::ResizeNESW,
                                                                    pal::chipResize, theme),
                                                    cursorSwatchRow("ResizeNWSE", Cursor::ResizeNWSE,
                                                                    pal::chipResize, theme),
                                                    cursorSwatchRow("ResizeAll", Cursor::ResizeAll,
                                                                    pal::chipHand, theme),
                                                    cursorSwatchRow("Crosshair", Cursor::Crosshair,
                                                                    pal::chipDefault, theme),
                                                    cursorSwatchRow("NotAllowed", Cursor::NotAllowed,
                                                                    pal::chipWarn, theme),

                                                    HStack{
                                                        .spacing = 12.f,
                                                        .vAlign = VerticalAlignment::Center,
                                                        .children = children(
                                                                Text{
                                                                    .text = "Text + IBeam",
                                                                    .style = theme.typeLabel,
                                                                    .color = pal::label,
                                                                }
                                                                    .size(152.f, 0.f),
                                                                Text{
                                                                    .text = "Read-only label — cursor is I-beam over "
                                                                            "glyphs (no pointer handlers).",
                                                                    .style = theme.typeBody,
                                                                    .color = pal::label,
                                                                    .wrapping = TextWrapping::Wrap,
                                                                }
                                                                    .cursor(Cursor::IBeam)
                                                                    .padding(10.f)
                                                                    .background(FillStyle::solid(Color::hex(0xFFFFFF)))
                                                                    .border(StrokeStyle::solid(Color::hex(0xC8C8D0), 1.f))
                                                                    .cornerRadius(CornerRadius(8.f))
                                                                    .flex(1.f)
                                                            ),
                                                    },

                                                    HStack{
                                                        .spacing = 0.f,
                                                        .children = children(
                                                                VStack{
                                                                            .spacing = 8.f,
                                                                            .hAlign = HorizontalAlignment::Leading,
                                                                            .children = children(
                                                                                    Text{
                                                                                        .text = "Drag lock (ResizeAll)",
                                                                                        .style = theme.typeHeading,
                                                                                        .color = pal::label,
                                                                                    },
                                                                                    Rectangle{
                                                                                        .fill = FillStyle::solid(pal::chipAccent),
                                                                                    }
                                                                                        .height(52.f)
                                                                                        .cursor(Cursor::ResizeAll)
                                                                                        .onPointerDown([](Point) {})
                                                                                        .onPointerUp([](Point) {})
                                                                                        .onPointerMove([](Point) {})
                                                                                        .cornerRadius(CornerRadius(10.f))
                                                                                ),
                                                                        }
                                                                    .flex(1.f)
                                                            ),
                                                    }
                                                ),
                                        }.padding(20.f)
                                    ),
                            }
                        ),
                }
            ),
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {520, 640},
      .title = "Flux — Cursor demo",
      .resizable = true,
  });
  w.setView<CursorDemo>();
  return app.exec();
}
