// Showcases VStack (vertical) and HStack (horizontal) layout: spacing,
// alignment, nesting, and Spacer.
#include <cstdio>

#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
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
        Theme const& theme = useEnvironment<Theme>();
        return ScrollView {
            .axis = ScrollAxis::Vertical,
            .children = children(
                VStack {
                    .spacing = 22.f,
                    .hAlign = HorizontalAlignment::Leading,
                    .children = children(
                        Text {
                            .text = "HStack & VStack",
                            .style = theme.typeDisplay,
                            .color = pal::ink
                        },
                        Text {
                            .text = "VStack arranges children top-to-bottom; HStack left-to-right. Use spacing, padding, and alignment to tune layout.",
                            .style = theme.typeBody,
                            .color = pal::muted,
                            .wrapping = TextWrapping::Wrap
                        },
                        Text {
                            .text = "VStack",
                            .style = theme.typeHeading,
                            .color = pal::ink
                        },
                        VStack {
                            .spacing = 10.f,
                            .hAlign = HorizontalAlignment::Center,
                            .children = children(
                                Rectangle {
                                    .fill = FillStyle::solid(pal::coral),
                                }.size(160.f, 36.f).cornerRadius(CornerRadius{8.f}),
                                Rectangle {
                                    .fill = FillStyle::solid(pal::teal),
                                }.size(200.f, 36.f).cornerRadius(CornerRadius{8.f}),
                                Rectangle {
                                    .fill = FillStyle::solid(pal::indigo),
                                }.size(120.f, 36.f).cornerRadius(CornerRadius{8.f})
                            ),
                        },
                        Text {
                            .text = "HStack",
                            .style = theme.typeHeading,
                            .color = pal::ink
                        },
                        HStack {
                            .spacing = 12.f,
                            .vAlign = VerticalAlignment::Center,
                            .children = children(
                                Rectangle {
                                    .fill = FillStyle::solid(pal::coral),
                                }.size(56.f, 56.f).cornerRadius(CornerRadius{10.f}).flex(2.f, 1.f, 0.f),
                                Rectangle {
                                    .fill = FillStyle::solid(pal::teal),
                                }.size(56.f, 72.f).cornerRadius(CornerRadius{10.f}),
                                Rectangle {
                                    .fill = FillStyle::solid(pal::indigo),
                                }.size(56.f, 40.f).cornerRadius(CornerRadius{10.f}).flex(1.f, 1.f, 0.f),
                                Rectangle {
                                    .fill = FillStyle::solid(pal::amber),
                                }.size(56.f, 56.f).cornerRadius(CornerRadius{10.f})
                            ),
                        },
                        Text {
                            .text = "Nested — HStack rows inside a VStack",
                            .style = theme.typeHeading,
                            .color = pal::ink
                        },
                        VStack {
                            .spacing = 8.f,
                            .hAlign = HorizontalAlignment::Leading,
                            .children = children(
                                HStack {
                                    .spacing = 8.f,
                                    .vAlign = VerticalAlignment::Center,
                                    .children = children(
                                        Rectangle {
                                            .fill = FillStyle::solid(Color::hex(0xE8E8EF)),
                                        }.height(28.f).cornerRadius(CornerRadius{6.f}).flex(1.f),
                                        Text {
                                            .text = "A",
                                            .style = theme.typeBody,
                                            .color = pal::ink,
                                        }.padding(6.f)
                                    ),
                                },
                                HStack {
                                    .spacing = 8.f,
                                    .vAlign = VerticalAlignment::Center,
                                    .children = children(
                                        Rectangle {
                                            .fill = FillStyle::solid(Color::hex(0xE8E8EF)),
                                        }.height(28.f).cornerRadius(CornerRadius{6.f}).flex(1.f),
                                        Text {
                                            .text = "B",
                                            .style = theme.typeBody,
                                            .color = pal::ink,
                                        }.padding(6.f)
                                    ),
                                },
                                HStack {
                                    .spacing = 8.f,
                                    .vAlign = VerticalAlignment::Center,
                                    .children = children(
                                        Rectangle {
                                            .fill = FillStyle::solid(Color::hex(0xE8E8EF)),
                                        }.height(28.f).cornerRadius(CornerRadius{6.f}).flex(1.f),
                                        Text {
                                            .text = "C",
                                            .style = theme.typeBody,
                                            .color = pal::ink,
                                        }.padding(6.f)
                                    ),
                                }
                            ),
                        },
                        Text {
                            .text = "HStack + Spacer (flex along the row)",
                            .style = theme.typeHeading,
                            .color = pal::ink
                        },
                        HStack {
                            .spacing = 0.f,
                            .vAlign = VerticalAlignment::Center,
                            .children = children(
                                Text {
                                    .text = "Leading",
                                    .style = theme.typeLabel,
                                    .color = pal::ink
                                },
                                Spacer {},
                                Text {
                                    .text = "Trailing",
                                    .style = theme.typeLabel,
                                    .color = pal::ink
                                }
                            ),
                        }
                    ),
                }.padding(24.f)
            ),
        };
    }
};

int main(int argc, char *argv[]) {
    Application app(argc, argv);
    auto &w = app.createWindow<Window>({
        .title = "Flux — HStack & VStack"
    });
    w.setView<StackDemoRoot>();
    return app.exec();
}
