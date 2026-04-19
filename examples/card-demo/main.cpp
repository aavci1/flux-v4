#include <Flux.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Icon.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace flux;

namespace {

/// Demo-only: `CardListView` needs live window width for `TextSystem::measure`. A proper fix is framework
/// support for passing context (e.g. `Window` or size) through the component tree (environment / context).
Window* gCardDemoWindow = nullptr;

} // namespace

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
  /// Width available to the card row (matches parent content width). Used for wrapped body measurement.
  float availableWidth = 400.f;

  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    auto expanded = useState<bool>(false);
    auto bodyOpacity = useAnimation<float>(0.f);

    float const innerTextWidth = std::max(1.f, availableWidth - 36.f);

    float const bodyTextHeight = useMemo([&] {
      TextSystem& ts = Application::instance().textSystem();
      Font const bodyFont = theme.bodyFont;
      TextLayoutOptions opts{.wrapping = TextWrapping::Wrap};
      return ts.measure(detail, bodyFont, pal::sublabel, innerTextWidth, opts).height;
    }, detail, availableWidth);

    std::vector<Element> rows;
    rows.emplace_back(HStack {
        .spacing = 12.f,
        .alignment = Alignment::Center,
        .children = children(
            Rectangle {}
                .fill(FillStyle::solid(accent))
                .size(14.f, 14.f)
                .cornerRadius(7.f),
            Text {
                .text = title,
                .font = Font::title2(),
                .color = pal::label,
            }
                .size(0.f, 24.f)
                .flex(1.f),
            Icon {
                .name = expanded ? IconName::ExpandLess : IconName::ExpandMore,
                .size = theme.headlineFont.size,
            }
        ),
    });

    if (*expanded || *bodyOpacity > 0.001f) {
      rows.emplace_back(HStack{
          .spacing = 0.f,
          .children = children(
                  Text{
                      .text = detail,
                      .font = Font::body(),
                      .color = pal::sublabel,
                      .wrapping = TextWrapping::Wrap,
                  }
                      .size(0.f, *bodyOpacity * bodyTextHeight)
                      .opacity(*bodyOpacity)
                      .flex(1.f)
              ),
      });
    }

    auto handleTap = [expanded, bodyOpacity] {
      bool const next = !*expanded;
      expanded = next;
      WithTransition t{Transition::spring(500.f, 25.f, 0.5f)};
      bodyOpacity = next ? 1.f : 0.f;
    };

    return VStack {
        .spacing = theme.space4,
        .children = std::move(rows)
    }
        .fill(FillStyle::solid(Color::controlBackground()))
        .stroke(StrokeStyle::solid(Color::opaqueSeparator(), 1.f))
        .cornerRadius(theme.radiusXLarge)
        .padding(theme.space4)
        .cursor(Cursor::Hand)
        .onTap(handleTap);
  }
};

struct CardListView {
    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        float const listContentWidth = gCardDemoWindow ? std::max(1.f, gCardDemoWindow->getSize().width - 48.f) : 432.f;

        return ScrollView {
            .axis = ScrollAxis::Vertical,
            .children = children(
                VStack {
                    .spacing = theme.space3,
                    .alignment = Alignment::Stretch,
                    .children = children(
                        Text {
                            .text = "Flux Components",
                            .font = Font::largeTitle(),
                            .color = Color::primary(),
                            .horizontalAlignment = HorizontalAlignment::Leading,
                        },
                        Text {
                            .text = "Tap a card to expand",
                            .font = Font::body(),
                            .color = Color::secondary(),
                            .horizontalAlignment = HorizontalAlignment::Leading,
                            .wrapping = TextWrapping::Wrap,
                        },
                        Card {
                            .accent = Color::accent(),
                            .title = "Metal Renderer",
                            .detail = "SDF rounded-rect shaders, glyph atlas, libtess2.",
                            .availableWidth = listContentWidth
                        },
                        Card {
                            .accent = Color::accent(),
                            .title = "Reactive State",
                            .detail = "Signal<T>, Computed<T>, Animation<T>.",
                            .availableWidth = listContentWidth
                        },
                        Card {
                            .accent = Color::accent(),
                            .title = "Scene Tree",
                            .detail = "Retained nodes, keyed reconciliation, hit testing.",
                            .availableWidth = listContentWidth
                        }
                    ),
                }
                    .padding(24.f)
            )
        };
    }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {800, 800},
      .title = "Flux — Card demo",
      .resizable = true,
  });
  gCardDemoWindow = &w;
  w.setView<CardListView>();
  return app.exec();
}
