#include <Flux.hpp>
#include <Flux/Graphics/AttributedString.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>

#include <algorithm>
#include <string>

using namespace flux;

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& w = app.createWindow({
      .size = {720, 640},
      .title = "Flux — Text demo",
  });

  w.setView(
    VStack {
      .spacing = 16.f,
      .children = {
        Text {
          .text = "Text in Flux",
          .font = {.size = 34.f, .weight = 600.f},
          .color = Color::rgb(18, 18, 24),
          .horizontalAlignment = HorizontalAlignment::Center,
        },
        Text {
          .text = "layout · measure · wrap · attributed runs",
          .font = {.size = 14.f, .weight = 400.f},
          .color = Color::rgb(110, 110, 125),
          .horizontalAlignment = HorizontalAlignment::Center,
        },
        Text {
          .text = "Line wrapping",
          .font = {.size = 13.f, .weight = 600.f},
          .color = Color::rgb(75, 75, 88),
        },
        HStack{
            .spacing = 0.f,
            .children =
                {
                    Text{
                            .text = "TextLayout uses the same Core Text framesetter constraints as measure, so box sizing and rendered glyphs stay in sync when maxWidth is set. Resize the window to see reflow.",
                            .font = {.size = 16.f, .weight = 420.f},
                            .color = Colors::darkGray,
                            .horizontalAlignment = HorizontalAlignment::Center,
                            .verticalAlignment = VerticalAlignment::Top,
                            .wrapping = TextWrapping::Wrap,
                        }
                        .padding(24.f)
                        .background(FillStyle::solid(Color::rgb(250, 250, 252)))
                        .border(StrokeStyle::solid(Color::rgb(200, 200, 210), 1.f))
                        .cornerRadius(CornerRadius(6.f, 6.f, 6.f, 6.f))
                        .flex(1.f),
                },
        },
        Text {
          .text = "Attributed string",
          .font = {.size = 13.f, .weight = 600.f},
          .color = Color::rgb(75, 75, 88),
        },
        HStack {
          .spacing = 16.f,
          .children = {
            Text {
              .text = "Swift",
              .font = {.size = 22.f, .weight = 520.f},
              .color = Colors::blue,
            },
            Text {
              .text = "UIKit",
              .font = {.size = 22.f, .weight = 520.f},
              .color = Color::rgb(180, 60, 50),
            },
            Text {
              .text = "AppKit",
              .font = {.size = 22.f, .weight = 520.f},
              .color = Color::rgb(40, 140, 75),
            },
          },
        },
        Text {
          .text = "firstBaseline → lastBaseline: layout metrics for alignment APIs.",
          .font = {.size = 12.f, .weight = 400.f},
          .color = Color::rgb(140, 140, 155),
        },
      },
    }.padding(24.f)
  );

  return app.exec();
}
