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

Element swatch(EnvironmentValue<Theme> theme, std::string label, Color Theme::* colorField) {
  Theme const& current = theme.peek();
  return VStack{
      .spacing = current.space2,
      .alignment = Alignment::Start,
      .children = children(
          Element{Rectangle{}}
              .size(132.f, 54.f)
              .fill(Reactive2::Bindable<Color>{[theme, colorField] {
                return theme().*colorField;
              }})
              .stroke(Reactive2::Bindable<Color>{[theme] {
                return theme().separatorColor;
              }}, Reactive2::Bindable<float>{1.f})
              .cornerRadius(current.radiusMedium),
          Text{
              .text = std::move(label),
              .font = Font::footnote(),
              .color = Color::secondary(),
          })};
}

struct ThemeDemoRoot {
  Element body() const {
    auto themeSignal = useEnvironment<Theme>();
    Theme const& theme = themeSignal.peek();
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

    std::vector<Element> chips;
    chips.push_back(swatch(themeSignal, "accentColor", &Theme::accentColor));
    chips.push_back(swatch(themeSignal, "labelColor", &Theme::labelColor));
    chips.push_back(swatch(themeSignal, "windowBackgroundColor", &Theme::windowBackgroundColor));
    chips.push_back(swatch(themeSignal, "controlBackgroundColor", &Theme::controlBackgroundColor));

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
                        .text = "Toggle Theme",
                        .font = Font::headline(),
                        .color = Color::primary(),
                    }.padding(theme.space3)
                     .fill(Color::controlBackground())
                     .stroke(Color::separator(), 1.f)
                     .cornerRadius(theme.radiusFull)
                     .onTap([themeSignal, density] {
                       bool const isDark = themeSignal.peek().windowBackgroundColor == Theme::dark().windowBackgroundColor;
                       Theme next = isDark ? Theme::light() : Theme::dark();
                       themeSignal.set(next.withDensity(*density));
                     }),
                    Text{
                        .text = "Density " + std::to_string(*density),
                        .font = Font::headline(),
                        .color = Color::primary(),
                    }.padding(theme.space3)
                     .fill(Color::controlBackground())
                     .stroke(Color::separator(), 1.f)
                     .cornerRadius(theme.radiusFull)
                     .onTap([density, themeSignal] {
                       density = *density > 1.f ? 0.75f : 1.25f;
                       themeSignal.set(themeSignal.peek().withDensity(*density));
                     }))})}
        .padding(theme.space7)
        .fill(Reactive2::Bindable<Color>{[themeSignal] {
          return themeSignal().windowBackgroundColor;
        }});
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
