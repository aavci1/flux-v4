#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/For.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace flux;

namespace {

struct Row {
  int id = 0;
  std::string title;
  Color color;

  bool operator==(Row const&) const = default;
};

std::vector<Row> initialRows() {
  return {
      {1, "Vertical list viewport", Color::rgb(58, 118, 185)},
      {2, "Horizontal indicator track", Color::rgb(219, 116, 62)},
      {3, "Two-axis content well", Color::rgb(82, 154, 96)},
      {4, "Pinned overlay stripe", Color::rgb(166, 88, 174)},
      {5, "Wheel and drag target", Color::rgb(82, 132, 146)},
      {6, "Retained keyed row", Color::rgb(202, 154, 60)},
  };
}

struct ScrollDemoRoot {
  Element body() const {
    Theme const& theme = useEnvironment<Theme>();
    auto rows = useState(initialRows());

    return VStack{
        .spacing = theme.space5,
        .alignment = Alignment::Center,
        .children = children(
            Text{
                .text = "Flux v5 Scroll Demo",
                .font = Font::largeTitle(),
                .color = Color::primary(),
                .horizontalAlignment = HorizontalAlignment::Center,
            },
            Text{
                .text = "Keyed rows mount once; reversing the list preserves row scopes.",
                .font = Font::body(),
                .color = Color::secondary(),
                .horizontalAlignment = HorizontalAlignment::Center,
            },
            Text{
                .text = "Reverse Rows",
                .font = Font::headline(),
                .color = Color::primary(),
                .horizontalAlignment = HorizontalAlignment::Center,
            }.padding(theme.space3)
             .fill(Color::controlBackground())
             .stroke(Color::separator(), 1.f)
             .cornerRadius(theme.radiusFull)
             .onTap([rows] {
               auto next = rows.peek();
               std::reverse(next.begin(), next.end());
               rows.set(std::move(next));
             }),
            Element{For(
                rows.signal,
                [](Row const& row) { return row.id; },
                [theme](Row const& row, Reactive2::Signal<std::size_t> index) {
                  Reactive2::Bindable<float> width{[index] {
                    return 280.f + static_cast<float>(index.get()) * 18.f;
                  }};
                  return HStack{
                      .spacing = theme.space3,
                      .alignment = Alignment::Center,
                      .children = children(
                          Rectangle{}
                              .size(14.f, 42.f)
                              .fill(row.color)
                              .cornerRadius(theme.radiusSmall),
                          Text{
                              .text = row.title,
                              .font = Font::body(),
                              .color = Color::primary(),
                          })}.padding(theme.space3)
                         .width(std::move(width))
                         .fill(Color::controlBackground())
                         .stroke(Color::separator(), 1.f)
                         .cornerRadius(theme.radiusMedium);
                },
                theme.space3)}
                .height(360.f)
                .padding(theme.space4)
                .fill(Color::windowBackground())
                .stroke(Color::separator(), 1.f)
                .cornerRadius(theme.radiusLarge))}
        .padding(theme.space7)
        .fill(Color::windowBackground())
        .environment(theme);
  }
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& window = app.createWindow<Window>({
      .size = {820, 660},
      .title = "Flux v5 Scroll Demo",
      .resizable = true,
  });

  window.setView<ScrollDemoRoot>();
  return app.exec();
}
