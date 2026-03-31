// Demonstrates Slider: continuous, stepped, disabled, custom colors,
// custom sizing, keyboard arrows, and live value display.
#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Slider.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/ZStack.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <cstdio>
#include <string>

using namespace flux;

namespace {

std::string fmtInt(float v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(v));
  return buf;
}

std::string fmtPct(float v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(v * 100.f));
  return buf;
}

} // namespace

struct LabeledSlider {
  std::string label;
  std::string valueText;
  State<float> value{};
  float min = 0.f;
  float max = 1.f;
  float step = 0.f;
  bool disabled = false;
  Color activeColor = kFromTheme;

  auto body() const {
    FluxTheme const& theme = useEnvironment<FluxTheme>();

    return VStack{
        .spacing = 6.f,
        .children = {
            Element{HStack{
                .spacing = 8.f,
                .vAlign = VerticalAlignment::Center,
                .children = {
                    Text{
                        .text = label,
                        .font = theme.typeBody.toFont(),
                        .color = disabled ? theme.colorTextDisabled : theme.colorTextPrimary,
                    },
                    Spacer{},
                    Text{
                        .text = valueText,
                        .font = theme.typeLabel.toFont(),
                        .color = disabled ? theme.colorTextDisabled : theme.colorTextSecondary,
                    },
                },
            }}.withFlex(1.f),
            Element{Slider{
                .value = value,
                .min = min,
                .max = max,
                .step = step,
                .activeColor = activeColor,
                .disabled = disabled,
                .onChange = [label = label](float v) {
                  std::fprintf(stderr, "[slider-demo] %s → %.3f\n", label.c_str(),
                               static_cast<double>(v));
                },
            }}.withFlex(1.f),
        },
    };
  }
};

struct SliderDemoRoot {
  auto body() const {
    FluxTheme const& theme = useEnvironment<FluxTheme>();

    auto opacity = useState(0.75f);
    auto volume = useState(50.f);
    auto rating = useState(3.f);
    auto disabled = useState(0.5f);
    auto green = useState(0.6f);
    auto compact = useState(0.5f);

    return ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children = {
            Rectangle{.fill = FillStyle::solid(theme.colorBackground)},
            VStack{
                .spacing = 20.f,
                .padding = 24.f,
                .hAlign = HorizontalAlignment::Leading,
                .children = {
                    Text{
                        .text = "Slider",
                        .font = theme.typeHeading.toFont(),
                        .color = theme.colorTextPrimary,
                    },
                    Element{Text{
                        .text = "Continuous and stepped range input "
                                "with drag, keyboard arrows, focus "
                                "ring, and thumb scale feedback.",
                        .font = theme.typeBody.toFont(),
                        .color = theme.colorTextSecondary,
                        .wrapping = TextWrapping::Wrap,
                    }}.withFlex(1.f),

                    Text{
                        .text = "Continuous",
                        .font = theme.typeSubtitle.toFont(),
                        .color = theme.colorTextPrimary,
                    },
                    Element{LabeledSlider{
                        .label = "Opacity",
                        .valueText = fmtPct(*opacity),
                        .value = opacity,
                    }}.withFlex(1.f),

                    Text{
                        .text = "Stepped",
                        .font = theme.typeSubtitle.toFont(),
                        .color = theme.colorTextPrimary,
                    },
                    Element{LabeledSlider{
                        .label = "Volume",
                        .valueText = fmtInt(*volume),
                        .value = volume,
                        .min = 0.f,
                        .max = 100.f,
                        .step = 1.f,
                    }}.withFlex(1.f),

                    Element{LabeledSlider{
                        .label = "Rating",
                        .valueText = fmtInt(*rating) + " / 5",
                        .value = rating,
                        .min = 1.f,
                        .max = 5.f,
                        .step = 1.f,
                    }}.withFlex(1.f),

                    Text{
                        .text = "Disabled",
                        .font = theme.typeSubtitle.toFont(),
                        .color = theme.colorTextPrimary,
                    },
                    Element{LabeledSlider{
                        .label = "Locked",
                        .valueText = fmtPct(*disabled),
                        .value = disabled,
                        .disabled = true,
                    }}.withFlex(1.f),

                    Text{
                        .text = "Custom color",
                        .font = theme.typeSubtitle.toFont(),
                        .color = theme.colorTextPrimary,
                    },
                    Element{LabeledSlider{
                        .label = "Green accent",
                        .valueText = fmtPct(*green),
                        .value = green,
                        .activeColor = theme.colorSuccess,
                    }}.withFlex(1.f),

                    Text{
                        .text = "Custom sizing",
                        .font = theme.typeSubtitle.toFont(),
                        .color = theme.colorTextPrimary,
                    },
                    Element{HStack{
                        .spacing = 8.f,
                        .vAlign = VerticalAlignment::Center,
                        .children = {
                            Text{
                                .text = "Compact",
                                .font = theme.typeBody.toFont(),
                                .color = theme.colorTextPrimary,
                            },
                            Spacer{},
                            Text{
                                .text = fmtPct(*compact),
                                .font = theme.typeLabel.toFont(),
                                .color = theme.colorTextSecondary,
                            },
                        },
                    }}.withFlex(1.f),
                    Element{Slider{
                        .value = compact,
                        .trackHeight = 3.f,
                        .thumbSize = 14.f,
                    }}.withFlex(1.f),

                    Text{
                        .text = "Live preview",
                        .font = theme.typeSubtitle.toFont(),
                        .color = theme.colorTextPrimary,
                    },
                    Rectangle{
                        .frame = {0.f, 0.f, 0.f, 48.f},
                        .cornerRadius = CornerRadius{theme.radiusMedium},
                        .fill = FillStyle::solid(Color{
                            theme.colorAccent.r,
                            theme.colorAccent.g,
                            theme.colorAccent.b,
                            *opacity,
                        }),
                        .stroke = StrokeStyle::solid(theme.colorBorder, 1.f),
                        .flexGrow = 1.f,
                    },
                },
            },
        },
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {480, 780},
      .title = "Flux — Slider demo",
      .resizable = true,
  });
  w.setView<SliderDemoRoot>();
  return app.exec();
}
