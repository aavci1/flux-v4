// Showcases VStack (vertical) and HStack (horizontal) layout: spacing,
// alignment, nesting, and Spacer.
#include <cstdio>

#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

using namespace flux;

namespace pal {
constexpr Color bg = Color::hex(0xF2F2F7);
constexpr Color ink = Color::hex(0x111118);
constexpr Color muted = Color::hex(0x6E6E80);
constexpr Color coral = Color::hex(0xE85D4C);
constexpr Color teal = Color::hex(0x2A9D8F);
constexpr Color indigo = Color::hex(0x4361EE);
constexpr Color amber = Color::hex(0xF4A261);
} // namespace pal

struct StackDemoRoot {
    auto body() const {
        return ScrollView {
            .axis = ScrollAxis::Vertical,
            .flexGrow = 1.f,
            .children = {
                VStack {
                    .spacing = 22.f,
                    .padding = 24.f,
                    .hAlign = HorizontalAlignment::Leading,
                    .children = {
                        Text {
                            .text = "HStack & VStack",
                            .font = {.size = 26.f, .weight = 700.f},
                            .color = pal::ink
                        },
                        Text {
                            .text = "VStack arranges children top-to-bottom; HStack left-to-right. Use spacing, padding, and alignment to tune layout.",
                            .font = {.size = 14.f, .weight = 400.f},
                            .color = pal::muted,
                            .wrapping = TextWrapping::Wrap
                        },
                        Text {
                            .text = "VStack",
                            .font = {.size = 15.f, .weight = 600.f},
                            .color = pal::ink
                        },
                        VStack {
                            .spacing = 10.f,
                            .hAlign = HorizontalAlignment::Center,
                            .children =
                                children(
                                    Rectangle {
                                        .offsetX = 0.f, .offsetY = 0.f, .width = 160.f, .height = 36.f,
                                        .cornerRadius = CornerRadius{8.f},
                                        .fill = FillStyle::solid(pal::coral),
                                    },
                                    Rectangle {
                                        .offsetX = 0.f, .offsetY = 0.f, .width = 200.f, .height = 36.f,
                                        .cornerRadius = CornerRadius{8.f},
                                        .fill = FillStyle::solid(pal::teal),
                                    },
                                    Rectangle {
                                        .offsetX = 0.f, .offsetY = 0.f, .width = 120.f, .height = 36.f,
                                        .cornerRadius = CornerRadius{8.f},
                                        .fill = FillStyle::solid(pal::indigo),
                                    }),
                        },
                        Text {
                            .text = "HStack",
                            .font = {.size = 15.f, .weight = 600.f},
                            .color = pal::ink
                        },
                        HStack {
                            .spacing = 12.f,
                            .vAlign = VerticalAlignment::Center,
                            .children = {
                                Rectangle {
                                    .offsetX = 0.f, .offsetY = 0.f, .width = 56.f, .height = 56.f,
                                    .cornerRadius = CornerRadius{10.f},
                                    .fill = FillStyle::solid(pal::coral),
                                }.flex(2.f),
                                Rectangle {
                                    .offsetX = 0.f, .offsetY = 0.f, .width = 56.f, .height = 72.f,
                                    .cornerRadius = CornerRadius{10.f},
                                    .fill = FillStyle::solid(pal::teal),
                                },
                                Rectangle {
                                    .offsetX = 0.f, .offsetY = 0.f, .width = 56.f, .height = 40.f,
                                    .cornerRadius = CornerRadius{10.f},
                                    .fill = FillStyle::solid(pal::indigo),
                                }.flex(1.f),
                                Rectangle {
                                    .offsetX = 0.f, .offsetY = 0.f, .width = 56.f, .height = 56.f,
                                    .cornerRadius = CornerRadius{10.f},
                                    .fill = FillStyle::solid(pal::amber),
                                },
                            },
                        },
                        Text {
                            .text = "Nested — HStack rows inside a VStack",
                            .font = {.size = 15.f, .weight = 600.f},
                            .color = pal::ink
                        },
                        VStack {
                            .spacing = 8.f,
                            .hAlign = HorizontalAlignment::Leading,
                            .children = {
                                HStack {
                                    .spacing = 8.f,
                                    .vAlign = VerticalAlignment::Center,
                                    .children = {
                                        Rectangle {
                                            .offsetX = 0.f, .offsetY = 0.f, .width = 0.f, .height = 28.f,
                                            .cornerRadius = CornerRadius{6.f},
                                            .fill = FillStyle::solid(Color::hex(0xE8E8EF)),
                                            .flexGrow = 1.f,
                                        },
                                        Text {
                                            .text = "A",
                                            .font = {.size = 13.f, .weight = 600.f},
                                            .color = pal::ink,
                                        }
                                            .padding(6.f),
                                    },
                                },
                                HStack {
                                    .spacing = 8.f,
                                    .vAlign = VerticalAlignment::Center,
                                    .children = {
                                        Rectangle {
                                            .offsetX = 0.f, .offsetY = 0.f, .width = 0.f, .height = 28.f,
                                            .cornerRadius = CornerRadius{6.f},
                                            .fill = FillStyle::solid(Color::hex(0xE8E8EF)),
                                            .flexGrow = 1.f,
                                        },
                                        Text {
                                            .text = "B",
                                            .font = {.size = 13.f, .weight = 600.f},
                                            .color = pal::ink,
                                        }
                                            .padding(6.f),
                                    },
                                },
                                HStack {
                                    .spacing = 8.f,
                                    .vAlign = VerticalAlignment::Center,
                                    .children = {
                                        Rectangle {
                                            .offsetX = 0.f, .offsetY = 0.f, .width = 0.f, .height = 28.f,
                                            .cornerRadius = CornerRadius{6.f},
                                            .fill = FillStyle::solid(Color::hex(0xE8E8EF)),
                                            .flexGrow = 1.f,
                                        },
                                        Text {
                                            .text = "C",
                                            .font = {.size = 13.f, .weight = 600.f},
                                            .color = pal::ink,
                                        }
                                            .padding(6.f),
                                    },
                                },
                            },
                        },
                        Text {
                            .text = "HStack + Spacer (flex along the row)",
                            .font = {.size = 15.f, .weight = 600.f},
                            .color = pal::ink
                        },
                        HStack {
                            .spacing = 0.f,
                            .vAlign = VerticalAlignment::Center,
                            .children = {
                                Text {
                                    .text = "Leading",
                                    .font = {.size = 14.f, .weight = 500.f},
                                    .color = pal::ink
                                },
                                Spacer {},
                                Text {
                                    .text = "Trailing",
                                    .font = {.size = 14.f, .weight = 500.f},
                                    .color = pal::ink
                                },
                            },
                        },
                    },
                },
            },
        };
    }
};

int main(int argc, char *argv[]) {
    Application app(argc, argv);
    auto &w = app.createWindow<Window>({
        .size = {420, 640},
        .title = "Flux — HStack & VStack",
        .resizable = true,
    });
    w.setView<StackDemoRoot>();
    return app.exec();
}
