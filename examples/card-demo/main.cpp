#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>

#include <string>
#include <vector>

using namespace flux;

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------

namespace pal {
constexpr Color bg        = Color::hex(0xF2F2F7);
constexpr Color surface   = Color::hex(0xFFFFFF);
constexpr Color border    = Color::hex(0xE0E0E6);
constexpr Color label     = Color::hex(0x111118);
constexpr Color sublabel  = Color::hex(0x6E6E80);
constexpr Color accent0   = Color::hex(0x3A7BD5); // Metal Renderer — blue
constexpr Color accent1   = Color::hex(0x2E9E5B); // Reactive State  — green
constexpr Color accent2   = Color::hex(0xD05A2B); // Scene Graph     — orange
} // namespace pal

// ---------------------------------------------------------------------------
// Card — composite; signals live on this object (stable address via root holder).
// `mutable` so `body() const` can drive `set` from tap handlers.
// ---------------------------------------------------------------------------

struct Card {
  mutable Signal<bool>    expanded{false};
  mutable Animated<float> bodyOpacity{0.f};
  Color                   accent = pal::accent0;
  std::string             title;
  std::string             detail;

  auto body() const {
    float const op = bodyOpacity.get();

    auto dot = Rectangle{
        .frame        = {0, 0, 14.f, 14.f},
        .cornerRadius = CornerRadius(7.f),
        .fill         = FillStyle::solid(accent),
    };

    bool const open = expanded.get();
    auto chevron = Text{
        .text  = open ? "⌄" : "›",
        .font  = {.size = 24.f, .weight = 600.f},
        .color = pal::sublabel,
        .horizontalAlignment = HorizontalAlignment::Center,
        .verticalAlignment   = VerticalAlignment::Center,
        .frame = {0, 0, 24.f, 24.f},
    };

    auto header = HStack{
        .spacing  = 12.f,
        .padding  = 0.f,
        .vAlign   = VerticalAlignment::Center,
        .children = {
            std::move(dot),
            Text{
                .text   = title,
                .font   = {.size = 17.f, .weight = 600.f},
                .color  = pal::label,
            },
            Spacer{},
            std::move(chevron),
        },
    };

    float const expandedH = 56.f;
    float const bodyH     = op * expandedH;

    std::vector<Element> rows;
    rows.emplace_back(std::move(header));

    if (bodyH > 0.5f) {
      rows.emplace_back(Text{
          .text   = detail,
          .font   = {.size = 15.f, .weight = 400.f},
          .color  = pal::sublabel,
          .horizontalAlignment = HorizontalAlignment::Leading,
          .verticalAlignment   = VerticalAlignment::Top,
          .wrapping = TextWrapping::Wrap,
          .frame  = {0, 0, 0, bodyH},
      });
    }

    return ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children = {
            Rectangle{
                .cornerRadius = CornerRadius(14.f),
                .fill   = FillStyle::solid(pal::surface),
                .stroke = StrokeStyle::solid(pal::border, 1.f),
                .onTap  = [this] {
                  bool const next = !expanded.get();
                  expanded.set(next);
                  WithTransition t{Transition::spring(280.f, 22.f, 0.5f)};
                  bodyOpacity.set(next ? 1.f : 0.f);
                },
            },
            VStack{
                .spacing  = 10.f,
                .padding  = 18.f,
                .children = std::move(rows),
            },
        },
    };
  }
};

// ---------------------------------------------------------------------------
// Root — default member initializers + setView<CardListView>() avoid moving Cards
// (Signal / Animated are non-movable).
// ---------------------------------------------------------------------------

struct CardListView {
  Card card0{.accent = pal::accent0,
             .title  = "Metal Renderer",
             .detail = "SDF rounded-rect shaders, glyph atlas, path tessellation via libtess2."};
  Card card1{.accent = pal::accent1,
             .title  = "Reactive State",
             .detail = "Signal<T>, Computed<T>, Animated<T>. One timer drives all animations."};
  Card card2{.accent = pal::accent2,
             .title  = "Scene Graph",
             .detail = "Slot-map NodeStore, LayerNode compositing, HitTester for input routing."};

  auto body() const {
    return ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children = {
            Rectangle{
                .fill = FillStyle::solid(pal::bg),
            },
            VStack{
                .spacing  = 12.f,
                .padding  = 24.f,
                .hAlign   = HorizontalAlignment::Leading,
                .children = {
                    Text{
                        .text  = "Flux Components",
                        .font  = {.size = 28.f, .weight = 700.f},
                        .color = pal::label,
                    },
                    Text{
                        .text  = "Tap a card to expand",
                        .font  = {.size = 14.f, .weight = 400.f},
                        .color = pal::sublabel,
                    },
                    card0.body(),
                    card1.body(),
                    card2.body(),
                },
            },
        },
    };
  }
};

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& w = app.createWindow<Window>({
      .size      = {480, 560},
      .title     = "Flux — Card demo",
      .resizable = true,
  });

  w.setView<CardListView>();

  return app.exec();
}
