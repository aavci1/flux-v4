#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>

#include <sstream>
#include <string>

using namespace flux;

namespace pal {
constexpr Color bg = Color::hex(0xF2F2F7);
constexpr Color surface = Color::hex(0xFFFFFF);
constexpr Color border = Color::hex(0xD0D0D8);
constexpr Color label = Color::hex(0x111118);
constexpr Color sub = Color::hex(0x6E6E80);
constexpr Color accent = Color::hex(0x3A7BD5);
} // namespace pal

struct ParseResult {
  int wordCount = 0;
  int charCount = 0;
};

static int gParseCalls = 0;
static int gRebuildCount = 0;

ParseResult expensiveParse(std::string const& text) {
  ++gParseCalls;
  ParseResult r;
  r.charCount = static_cast<int>(text.size());
  std::istringstream ss{text};
  std::string word;
  while (ss >> word) {
    ++r.wordCount;
  }
  return r;
}

/// `useHover()` compares the runtime hover leaf key to the *calling* component's key. Calling it
/// in `MemoDemo::body()` would use the root key, which prefixes every hover target — the whole
/// window would read as hovered. This wrapper scopes hover to the button only. For full
/// `useHover` / `usePress` patterns, see `examples/hover-demo/main.cpp`.
struct MemoHoverButton {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    bool const hovered = useHover();
    Color const btnFill = hovered ? pal::accent : pal::surface;
    Color const btnText = hovered ? Color::hex(0xFFFFFF) : pal::label;
    return ZStack{.children = children(
                      Rectangle{}
                          .fill(FillStyle::solid(btnFill))
                          .stroke(StrokeStyle::solid(pal::border, 1.f))
                          .height(36.f)
                          .cursor(Cursor::Hand)
                          .cornerRadius(8.f),
                      Text{.text = "Hover me (rebuild, no text change)",
                           .font = theme.fontBodySmall,
                           .color = btnText,
                       }
                          .padding(10.f)
                  )};
  }
};

struct MemoDemo {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    ++gRebuildCount;

    auto text = useState<std::string>("Hello, world");

    ParseResult const& result = useMemo([&] { return expensiveParse(*text); }, *text);

    int const cacheHits = gRebuildCount - gParseCalls;

    return ZStack{.children = children(
        Rectangle{}.fill(FillStyle::solid(pal::bg)),
        VStack{
            .spacing = 16.f,
            .alignment = Alignment::Start,
            .children = children(
                Text{.text = "useMemo demo",
                     .font = theme.fontDisplay,
                     .color = pal::label},

                HStack{
                    .spacing = 0.f,
                    .children = children(
                            Text{.text = "Content: " + *text,
                                 .font = theme.fontBody,
                                 .color = pal::label,
                                 .wrapping = TextWrapping::Wrap,
                             }
                                .padding(12.f)
                                .fill(FillStyle::solid(pal::surface))
                                .stroke(StrokeStyle::solid(pal::border, 1.f))
                                .flex(1.f)
                        ),
                },

                HStack{.spacing = 12.f, .children = children(
                    ZStack{.children = children(
                        Rectangle{}
                            .fill(FillStyle::solid(pal::accent))
                            .height(36.f)
                            .cursor(Cursor::Hand)
                            .onTap([text] { text = *text + " word"; })
                            .cornerRadius(8.f),
                        Text{.text = "+ Word",
                             .font = theme.fontLabel,
                             .color = theme.colorOnAccent,
                         }
                            .padding(10.f)
                    )},
                    MemoHoverButton{},
                    ZStack{.children = children(
                        Rectangle{}
                            .fill(FillStyle::solid(pal::surface))
                            .stroke(StrokeStyle::solid(pal::border, 1.f))
                            .height(36.f)
                            .cursor(Cursor::Hand)
                            .onTap([text] { text = std::string{}; })
                            .cornerRadius(8.f),
                        Text{.text = "Clear",
                             .font = theme.fontBody,
                             .color = pal::label,
                         }
                            .padding(10.f)
                    )}
                )},

                VStack{
                    .spacing = 6.f,
                    .alignment = Alignment::Start,
                    .children = children(
                        Text{.text = "Words (useMemo): " + std::to_string(result.wordCount),
                             .font = theme.fontBody,
                             .color = pal::label},
                        Text{.text = "Characters (useMemo): " + std::to_string(result.charCount),
                             .font = theme.fontBody,
                             .color = pal::label},
                        Text{.text = "Parse calls (expensive fn): " + std::to_string(gParseCalls),
                             .font = theme.fontBodySmall,
                             .color = pal::accent},
                        Text{.text = "Total rebuilds: " + std::to_string(gRebuildCount) +
                                         " (×2 per event: measure + build pass)",
                             .font = theme.fontBody,
                             .color = pal::sub},
                        Text{.text = "Cache hit ratio: " + std::to_string(cacheHits) + "/" +
                                         std::to_string(gRebuildCount) +
                                         " rebuilds skipped re-parse",
                             .font = theme.fontBodySmall,
                             .color = pal::sub}
                    ),
                }
            ),
        }.padding(24.f)
    )};
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {520, 480},
      .title = "Flux — useMemo demo",
      .resizable = true,
  });
  w.setView<MemoDemo>();
  return app.exec();
}
