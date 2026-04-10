#include <Flux.hpp>
#include <Flux/Graphics/AttributedString.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>

#include <algorithm>
#include <string>

using namespace flux;

struct TextDemoRoot {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    return VStack{
        .spacing = 16.f,
        .children = children(
            Text {
                .text = "Text in Flux",
                .style = theme.typeDisplay,
                .color = theme.colorTextPrimary,
                .horizontalAlignment = HorizontalAlignment::Center,
            },
            Text {
                .text = "layout · measure · wrap · attributed runs",
                .style = theme.typeBody,
                .color = theme.colorTextSecondary,
                .horizontalAlignment = HorizontalAlignment::Center,
            },
            Text {
                .text = "Line wrapping",
                .style = theme.typeHeading,
                .color = theme.colorTextPrimary,
            },
            HStack {
                .spacing = 0.f,
                .children = children(
                    Text {
                        .text = "TextLayout uses the same Core Text framesetter constraints as measure, so box "
                                "sizing and rendered glyphs stay in sync when maxWidth is set. Resize the window to see reflow.",
                        .style = theme.typeBody,
                        .color = theme.colorTextPrimary,
                        .horizontalAlignment = HorizontalAlignment::Center,
                        .verticalAlignment = VerticalAlignment::Top,
                        .wrapping = TextWrapping::Wrap,
                    }
                    .padding(24.f)
                    .fill(FillStyle::solid(Color::rgb(250, 250, 252)))
                    .stroke(StrokeStyle::solid(Color::rgb(200, 200, 210), 1.f))
                    .cornerRadius(6.f)
                    .flex(1.f)
                ),
            },
            Text {
                .text = "Attributed string",
                .style = theme.typeHeading,
                .color = theme.colorTextPrimary,
            },
            [&] {
              Font const f = theme.typeTitle.toFont();
              Color const sep = theme.colorTextSecondary;
              AttributedString rich;
              rich.utf8 = "Swift / UIKit / AppKit";
              rich.runs = {
                  {0, 5, f, Colors::blue},
                  {5, 8, f, sep},
                  {8, 13, f, Color::rgb(180, 60, 50)},
                  {13, 16, f, sep},
                  {16, 22, f, Color::rgb(40, 140, 75)},
              };
              return Text{
                  .style = theme.typeTitle,
                  .color = theme.colorTextPrimary,
                  .attributed = std::move(rich),
                  .horizontalAlignment = HorizontalAlignment::Center,
              };
            }(),
            Text {
                .text = "firstBaseline → lastBaseline: layout metrics for alignment APIs.",
                .style = theme.typeLabelSmall,
                .color = theme.colorTextMuted,
            }
        )
    }.padding(24.f);
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& w = app.createWindow({
      .size = {720, 640},
      .title = "Flux — Text demo",
  });

  w.setView<TextDemoRoot>();

  return app.exec();
}
