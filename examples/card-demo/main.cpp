#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>

#include <string>
#include <vector>

using namespace flux;

namespace pal {
constexpr Color bg = Color::hex(0xF2F2F7);
constexpr Color surface = Color::hex(0xFFFFFF);
constexpr Color border = Color::hex(0xE0E0E6);
constexpr Color label = Color::hex(0x111118);
constexpr Color sublabel = Color::hex(0x6E6E80);
constexpr Color accent0 = Color::hex(0x3A7BD5);
constexpr Color accent1 = Color::hex(0x2E9E5B);
constexpr Color accent2 = Color::hex(0xD05A2B);
} // namespace pal

struct Card {
  Color accent = pal::accent0;
  std::string title;
  std::string detail;

  auto body() const {
    auto expanded = useState<bool>(false);
    auto bodyOpacity = useAnimated<float>(0.f);

    std::vector<Element> rows;
    rows.emplace_back(HStack{
        .spacing = 12.f,
        .vAlign = VerticalAlignment::Center,
        .children = {
            Rectangle{
                .frame = {0, 0, 14.f, 14.f},
                .cornerRadius = CornerRadius(7.f),
                .fill = FillStyle::solid(accent),
            },
            Element{Text{
                .text = title,
                .font = {.size = 17.f, .weight = 600.f},
                .color = pal::label,
                .frame = {0, 0, 0, 24.f},
            }}.withFlex(1.f),
            Text{
                .text = expanded ? "⌄" : "›",
                .font = {.size = 24.f, .weight = 600.f},
                .color = pal::sublabel,
                .horizontalAlignment = HorizontalAlignment::Center,
                .verticalAlignment = VerticalAlignment::Center,
                .frame = {0, 0, 24.f, 24.f},
            },
        },
    });

    if (bodyOpacity > 0.5f) {
      rows.emplace_back(Text{
          .text = detail,
          .font = {.size = 15.f, .weight = 400.f},
          .color = pal::sublabel,
          .wrapping = TextWrapping::Wrap,
          .frame = {0, 0, 0, bodyOpacity * 56.f},
      });
    }

    return ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children = {
            Rectangle{
                .cornerRadius = CornerRadius(14.f),
                .fill = FillStyle::solid(pal::surface),
                .stroke = StrokeStyle::solid(pal::border, 1.f),
                .onTap = [expanded, bodyOpacity] {
                  bool const next = !expanded;
                  expanded = next;
                  WithTransition t{Transition::spring(500.f, 25.f, 0.5f)};
                  bodyOpacity = next ? 1.f : 0.f;
                },
            },
            VStack{.spacing = 10.f, .padding = 18.f, .children = std::move(rows)},
        },
    };
  }
};

struct CardListView {
  auto body() const {
    return ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children = {
            Rectangle{.fill = FillStyle::solid(pal::bg)},
            VStack{
                .spacing = 12.f,
                .padding = 24.f,
                .hAlign = HorizontalAlignment::Leading,
                .children = {
                    Text{.text = "Flux Components",
                         .font = {.size = 28.f, .weight = 700.f},
                         .color = pal::label},
                    Text{.text = "Tap a card to expand",
                         .font = {.size = 14.f, .weight = 400.f},
                         .color = pal::sublabel},
                    Card{.accent = pal::accent0,
                         .title = "Metal Renderer",
                         .detail = "SDF rounded-rect shaders, glyph atlas, libtess2."},
                    Card{.accent = pal::accent1,
                         .title = "Reactive State",
                         .detail = "Signal<T>, Computed<T>, Animated<T>."},
                    Card{.accent = pal::accent2,
                         .title = "Scene Graph",
                         .detail = "Slot-map NodeStore, LayerNode, HitTester."},
                },
            },
        },
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {480, 560},
      .title = "Flux — Card demo",
      .resizable = true,
  });
  w.setView<CardListView>();
  return app.exec();
}
