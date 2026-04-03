#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/ForEach.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <numeric>
#include <string>

using namespace flux;

namespace pal {
constexpr Color bg = Color::hex(0xF2F2F7);
constexpr Color label = Color::hex(0x111118);
} // namespace pal

/// Row with per-index hover state — stable across list length changes when rows are appended/removed at the end.
struct HoverRow {
  int index = 0;

  auto body() const {
    bool const hovered = useHover();
    Color const bgColor = hovered ? Color::hex(0xDFDFE6) : Color::hex(0xFFFFFF);

    return ZStack{.children = {
                      Rectangle{
                          .fill = FillStyle::solid(bgColor),
                          .stroke = StrokeStyle::solid(Color::hex(0xE0E0E6), 1.f),
                      }
                          .height(44.f)
                          .cursor(Cursor::Hand)
                          .cornerRadius(CornerRadius(8.f)),
                      Text{.text = "Row " + std::to_string(index) + (hovered ? "  ← hovered" : ""),
                           .font = {.size = 14.f, .weight = 400.f},
                           .color = pal::label,
                       }
                          .padding(14.f),
                  }};
  }
};

struct ForEachDemo {
  auto body() const {
    auto count = useState(5);

    std::vector<int> indices(static_cast<std::size_t>(*count));
    std::iota(indices.begin(), indices.end(), 0);

    return ZStack{
        .children =
            {
                Rectangle{.fill = FillStyle::solid(pal::bg)},
                VStack{
                    .spacing = 16.f,
                    .children =
                        {
                            Text{.text = "ForEach demo",
                                 .font = {.size = 22.f, .weight = 700.f},
                                 .color = pal::label},
                            HStack{
                                .spacing = 10.f,
                                .vAlign = VerticalAlignment::Center,
                                .children =
                                    {
                                        ZStack{
                                            .children =
                                                {
                                                    Rectangle{
                                                        .fill = FillStyle::solid(Color::hex(0x3A7BD5)),
                                                    }
                                                        .size(120.f, 36.f)
                                                        .cursor(Cursor::Hand)
                                                        .onTap([count] { count = *count + 1; })
                                                        .cornerRadius(CornerRadius(8.f)),
                                                    Text{.text = "+ Row",
                                                         .font = {.size = 14.f, .weight = 600.f},
                                                         .color = Color::hex(0xFFFFFF)},
                                                },
                                        },
                                        ZStack{
                                            .children =
                                                {
                                                    Rectangle{
                                                        .fill = FillStyle::solid(Color::hex(0xD05A2B)),
                                                    }
                                                        .size(120.f, 36.f)
                                                        .cursor(Cursor::Hand)
                                                        .onTap([count] {
                                                          if (*count > 0) {
                                                            count = *count - 1;
                                                          }
                                                        })
                                                        .cornerRadius(CornerRadius(8.f)),
                                                    Text{.text = "- Row",
                                                         .font = {.size = 14.f, .weight = 600.f},
                                                         .color = Color::hex(0xFFFFFF)},
                                                },
                                        },
                                    },
                            },
                            ScrollView{
                                .children =
                                    {
                                        VStack{
                                            .spacing = 6.f,
                                            .children =
                                                {
                                                    ForEach<int>(
                                                        std::move(indices),
                                                        [](int i) {
                                                          return Element{HoverRow{.index = i}};
                                                        },
                                                        6.f),
                                                },
                                        }.padding(12.f),
                                    },
                            },
                        },
                }.padding(20.f),
            },
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {420, 520},
      .title = "Flux — ForEach demo",
      .resizable = true,
  });
  w.setView<ForEachDemo>();
  return app.exec();
}
