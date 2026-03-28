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
// Card state — owns signals; non-copyable, heap-allocated via Element
// ---------------------------------------------------------------------------

struct CardState {
    Signal<bool>    expanded{false};
    Animated<float> bodyOpacity{0.f};

    CardState() = default;
    CardState(CardState const&) = delete;
    CardState& operator=(CardState const&) = delete;
};

// ---------------------------------------------------------------------------
// Card — composite component. Holds a raw pointer to CardState (which is
// owned by the parent AppState and outlives the component tree).
// ---------------------------------------------------------------------------

struct Card {
    CardState*  state   = nullptr;
    Color       accent  = pal::accent0;
    std::string title;
    std::string body;

    auto bodyView() const {
        float const op = state->bodyOpacity.get();

        // Indicator dot (coloured circle approximated by rounded rect)
        auto dot = Rectangle{
            .frame        = {0, 0, 14.f, 14.f},
            .cornerRadius = CornerRadius(7.f),
            .fill         = FillStyle::solid(accent),
        };

        // Chevron — rendered as a small ">" Text, rotated via opacity trick:
        // we don't have rotation on Text yet so we use "›" vs "⌄" swap.
        bool const open = state->expanded.get();
        auto chevron = Text{
            .text  = open ? "⌄" : "›",
            .font  = {.size = 18.f, .weight = 600.f},
            .color = pal::sublabel,
            .horizontalAlignment = HorizontalAlignment::Center,
            .verticalAlignment   = VerticalAlignment::Center,
            .frame = {0, 0, 20.f, 24.f},
        };

        // Header row: dot + title + spacer + chevron
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

        // Expanded body text — only contributes height when opacity > 0
        // We use a frame with height proportional to opacity so the card
        // animates open. At opacity=0 height=0, at opacity=1 height=natural.
        // Fixed row height for the expand animation — not measured layout height; long wrapped
        // lines may clip on narrow windows unless this is revisited.
        float const expandedH = 56.f;
        float const bodyH     = op * expandedH;

        std::vector<Element> rows;
        rows.emplace_back(std::move(header));

        if (bodyH > 0.5f) {
            rows.emplace_back(Text{
                .text   = body,
                .font   = {.size = 15.f, .weight = 400.f},
                .color  = pal::sublabel,
                .horizontalAlignment = HorizontalAlignment::Leading,
                .verticalAlignment   = VerticalAlignment::Top,
                .wrapping = TextWrapping::Wrap,
                .frame  = {0, 0, 0, bodyH}, // width 0 = fill available; height animated
            });
        }

        CardState* const st = state;
        return ZStack{
            .hAlign = HorizontalAlignment::Leading,
            .vAlign = VerticalAlignment::Top,
            .children = {
                // Card background
                Rectangle{
                    .cornerRadius = CornerRadius(14.f),
                    .fill   = FillStyle::solid(pal::surface),
                    .stroke = StrokeStyle::solid(pal::border, 1.f),
                    .onTap  = [st] {
                        bool const next = !st->expanded.get();
                        // Signal::set ignores transitions; schedule rebuild before animating opacity.
                        st->expanded.set(next);
                        WithTransition t{Transition::spring(280.f, 22.f, 0.5f)};
                        st->bodyOpacity.set(next ? 1.f : 0.f);
                    },
                },
                // Content
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
// App state — owns three CardStates with stable addresses
// ---------------------------------------------------------------------------

struct AppState {
    CardState cards[3];
};

// ---------------------------------------------------------------------------
// Root view — composite component
// ---------------------------------------------------------------------------

struct CardListView {
    AppState* state = nullptr;

    auto body() const {
        Card c0{state->cards + 0, pal::accent0,
                "Metal Renderer",
                "SDF rounded-rect shaders, glyph atlas, path tessellation via libtess2."};
        Card c1{state->cards + 1, pal::accent1,
                "Reactive State",
                "Signal<T>, Computed<T>, Animated<T>. One timer drives all animations."};
        Card c2{state->cards + 2, pal::accent2,
                "Scene Graph",
                "Slot-map NodeStore, LayerNode compositing, HitTester for input routing."};

        return ZStack{
            .hAlign = HorizontalAlignment::Leading,
            .vAlign = VerticalAlignment::Top,
            .children = {
                // Window background
                Rectangle{
                    .fill = FillStyle::solid(pal::bg),
                },
                // Card list
                VStack{
                    .spacing  = 12.f,
                    .padding  = 24.f,
                    .hAlign   = HorizontalAlignment::Leading,
                    .children = {
                        // Title
                        Text{
                            .text  = "Flux Components",
                            .font  = {.size = 28.f, .weight = 700.f},
                            .color = pal::label,
                        },
                        // Subtitle
                        Text{
                            .text  = "Tap a card to expand",
                            .font  = {.size = 14.f, .weight = 400.f},
                            .color = pal::sublabel,
                        },
                        // Cards
                        c0.bodyView(),
                        c1.bodyView(),
                        c2.bodyView(),
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
        .size     = {480, 560},
        .title    = "Flux — Card demo",
        .resizable = true,
    });

    // AppState lives in static storage for the duration of the app.
    // Its address is stable — CardState signal members are safe to capture.
    static AppState appState;

    w.setView(CardListView{&appState});

    return app.exec();
}
