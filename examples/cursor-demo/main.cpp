#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
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

Element cursorSwatchRow(std::string name, Cursor cursor, Color chip) {
  return HStack{
      .spacing = 0.f,
      .children =
          {
              HStack{
                    .spacing = 12.f,
                    .vAlign = VerticalAlignment::Center,
                    .children =
                        {
                            Text{.text = std::move(name),
                                 .font = {.size = 14.f, .weight = 600.f},
                                 .color = pal::label,
                                 .offsetX = 152.f, .offsetY = 0.f, .width = 0.f, .height = 0.f},
                            Rectangle{
                                .offsetX = 0.f, .offsetY = 0.f, .width = 0.f, .height = 44.f,
                                .cornerRadius = CornerRadius(8.f),
                                .fill = FillStyle::solid(chip),
                                .stroke = StrokeStyle::solid(Color::hex(0xC8C8D0), 1.f),
                                .flexGrow = 1.f,
                                .cursor = cursor,
                            },
                        },
                }
                  .withFlex(1.f),
          },
  };
}

} // namespace

/// Hover each row to see the platform cursor; drag the bottom strip to verify the cursor stays locked.
struct CursorDemo {
  auto body() const {
    return ZStack{
        .children =
            {
                Rectangle{.fill = FillStyle::solid(pal::bg)},
                VStack{
                    .spacing = 0.f,
                    .padding = 0.f,
                    .children =
                        {
                            VStack{
                                .spacing = 6.f,
                                .padding = 20.f,
                                .hAlign = HorizontalAlignment::Leading,
                                .children =
                                    {
                                        Text{.text = "Cursor shapes",
                                             .font = {.size = 26.f, .weight = 700.f},
                                             .color = pal::label},
                                        HStack{
                                            .spacing = 0.f,
                                            .children =
                                                {
                                                    Text{
                                                            .text = "Move the pointer over each swatch. Drag the resize strip — the "
                                                                    "cursor stays locked to that node during the drag.",
                                                            .font = {.size = 14.f, .weight = 400.f},
                                                            .color = pal::sub,
                                                            .wrapping = TextWrapping::Wrap,
                                                        }
                                                        .withFlex(1.f),
                                                },
                                        },
                                    },
                            },
                            ScrollView{
                                .axis = ScrollAxis::Vertical,
                                .flexGrow = 1.f,
                                .children =
                                    {
                                        VStack{
                                            .spacing = 10.f,
                                            .padding = 20.f,
                                            .children =
                                                {
                                                    cursorSwatchRow("Default", Cursor::Arrow,
                                                                    pal::chipDefault),
                                                    cursorSwatchRow("Hand", Cursor::Hand, pal::chipHand),
                                                    cursorSwatchRow("ResizeEW", Cursor::ResizeEW,
                                                                    pal::chipResize),
                                                    cursorSwatchRow("ResizeNS", Cursor::ResizeNS,
                                                                    pal::chipResize),
                                                    cursorSwatchRow("ResizeNESW", Cursor::ResizeNESW,
                                                                    pal::chipResize),
                                                    cursorSwatchRow("ResizeNWSE", Cursor::ResizeNWSE,
                                                                    pal::chipResize),
                                                    cursorSwatchRow("ResizeAll", Cursor::ResizeAll,
                                                                    pal::chipHand),
                                                    cursorSwatchRow("Crosshair", Cursor::Crosshair,
                                                                    pal::chipDefault),
                                                    cursorSwatchRow("NotAllowed", Cursor::NotAllowed,
                                                                    pal::chipWarn),

                                                    HStack{
                                                        .spacing = 12.f,
                                                        .vAlign = VerticalAlignment::Center,
                                                        .children =
                                                            {
                                                                Text{
                                                                    .text = "Text + IBeam",
                                                                    .font = {.size = 14.f, .weight = 600.f},
                                                                    .color = pal::label,
                                                                    .offsetX = 152.f, .offsetY = 0.f, .width = 0.f, .height = 0.f},
                                                                Text{
                                                                    .text = "Read-only label — cursor is I-beam over "
                                                                            "glyphs (no pointer handlers).",
                                                                    .font = {.size = 14.f, .weight = 400.f},
                                                                    .background = FillStyle::solid(
                                                                        Color::hex(0xFFFFFF)),
                                                                    .border = StrokeStyle::solid(
                                                                        Color::hex(0xC8C8D0), 1.f),
                                                                    .color = pal::label,
                                                                    .wrapping = TextWrapping::Wrap,
                                                                    .padding = 10.f,
                                                                    .cornerRadius = CornerRadius(8.f),
                                                                    .offsetX = 0.f, .offsetY = 0.f, .width = 0.f, .height = 0.f,
                                                                    .flexGrow = 1.f,
                                                                    .cursor = Cursor::IBeam,
                                                                }
                                                                    .withFlex(1.f),
                                                            },
                                                    },

                                                    HStack{
                                                        .spacing = 0.f,
                                                        .children =
                                                            {
                                                                VStack{
                                                                            .spacing = 8.f,
                                                                            .hAlign = HorizontalAlignment::Leading,
                                                                            .children =
                                                                                {
                                                                                    Text{
                                                                                        .text = "Drag lock (ResizeAll)",
                                                                                        .font = {.size = 14.f, .weight = 600.f},
                                                                                        .color = pal::label,
                                                                                    },
                                                                                    Rectangle{
                                                                                        .offsetX = 0.f, .offsetY = 0.f, .width = 0.f, .height = 52.f,
                                                                                        .cornerRadius = CornerRadius(10.f),
                                                                                        .fill = FillStyle::solid(pal::chipAccent),
                                                                                        .cursor = Cursor::ResizeAll,
                                                                                        .onPointerDown = [](Point) {},
                                                                                        .onPointerUp = [](Point) {},
                                                                                        .onPointerMove = [](Point) {},
                                                                                    },
                                                                                },
                                                                        }
                                                                    .withFlex(1.f),
                                                            },
                                                    },
                                                },
                                        },
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
      .size = {520, 640},
      .title = "Flux — Cursor demo",
      .resizable = true,
  });
  w.setView<CursorDemo>();
  return app.exec();
}
