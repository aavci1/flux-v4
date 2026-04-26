#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <string>
#include <vector>

using namespace flux;

namespace {

Element swatch(Theme const& theme, std::string label, Color color) {
  return VStack{
      .spacing = theme.space2,
      .alignment = Alignment::Start,
      .children = children(
          Rectangle{}
              .size(132.f, 54.f)
              .fill(color)
              .stroke(Color::separator(), 1.f)
              .cornerRadius(theme.radiusMedium),
          Text{
              .text = std::move(label),
              .font = Font::footnote(),
              .color = Color::secondary(),
          })};
}

struct ThemeDemoRoot {
  Element body() const {
    auto darkMode = useState(false);
    auto density = useState(1.0f);
    auto selected = useState(0);
    auto accentIndex = useState(0);
    auto search = useState(std::string{"Scope-owned state"});
    auto enabled = useState(true);
    auto amount = useState(0.72f);
    auto hoverPreview = useState(false);

    (void)selected;
    (void)accentIndex;
    (void)search;
    (void)enabled;
    (void)amount;
    (void)hoverPreview;

    Theme theme = *darkMode ? Theme::dark() : Theme::light();
    theme = theme.withDensity(*density);

    std::vector<Element> chips;
    chips.push_back(swatch(theme, "accentColor", theme.accentColor));
    chips.push_back(swatch(theme, "labelColor", theme.labelColor));
    chips.push_back(swatch(theme, "windowBackgroundColor", theme.windowBackgroundColor));
    chips.push_back(swatch(theme, "controlBackgroundColor", theme.controlBackgroundColor));

    return VStack{
        .spacing = theme.space6,
        .alignment = Alignment::Center,
        .children = children(
            Text{
                .text = "Flux v5 Theme",
                .font = Font::largeTitle(),
                .color = Color::primary(),
                .horizontalAlignment = HorizontalAlignment::Center,
            },
            Text{
                .text = "useState values are created once at mount; theme values flow through the environment.",
                .font = Font::body(),
                .color = Color::secondary(),
                .horizontalAlignment = HorizontalAlignment::Center,
                .wrapping = TextWrapping::Wrap,
            }.width(520.f),
            HStack{
                .spacing = theme.space4,
                .alignment = Alignment::Start,
                .children = std::move(chips),
            },
            HStack{
                .spacing = theme.space3,
                .alignment = Alignment::Center,
                .children = children(
                    Text{
                        .text = *darkMode ? "Dark" : "Light",
                        .font = Font::headline(),
                        .color = Color::primary(),
                    }.padding(theme.space3)
                     .fill(Color::controlBackground())
                     .stroke(Color::separator(), 1.f)
                     .cornerRadius(theme.radiusFull)
                     .onTap([darkMode] { darkMode = !*darkMode; }),
                    Text{
                        .text = "Density " + std::to_string(*density),
                        .font = Font::headline(),
                        .color = Color::primary(),
                    }.padding(theme.space3)
                     .fill(Color::controlBackground())
                     .stroke(Color::separator(), 1.f)
                     .cornerRadius(theme.radiusFull)
                     .onTap([density] { density = *density > 1.f ? 0.75f : 1.25f; }))})}
        .padding(theme.space7)
        .fill(Color::windowBackground())
        .environment(theme);
  }
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& window = app.createWindow<Window>({
      .size = {900, 620},
      .title = "Flux v5 Theme Demo",
      .resizable = true,
  });

  window.setView<ThemeDemoRoot>();
  return app.exec();
}
