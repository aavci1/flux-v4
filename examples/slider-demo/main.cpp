// RGB color picker: three sliders (0–255) with live swatch, hex, and rgb() text.
#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Slider.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/ZStack.hpp>
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

std::string fmtHex(float r, float g, float b) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", static_cast<int>(r), static_cast<int>(g),
                static_cast<int>(b));
  return buf;
}

std::string fmtRgbLine(float r, float g, float b) {
  char buf[48];
  std::snprintf(buf, sizeof(buf), "rgb(%d, %d, %d)", static_cast<int>(r), static_cast<int>(g),
                static_cast<int>(b));
  return buf;
}

constexpr float kChannelScale = 1.f / 255.f;

} // namespace

struct LabeledSlider {
  std::string label;
  std::string valueText;
  State<float> value{};
  float min = 0.f;
  float max = 255.f;
  float step = 1.f;
  Color activeColor = kColorFromTheme;

  auto body() const {
    Theme const& theme = useEnvironment<Theme>();

    return VStack{
        .spacing = 6.f,
        .children = children(
            HStack{
                .spacing = 8.f,
                .alignment = Alignment::Center,
                .children = children(
                    Text{
                        .text = label,
                        .style = theme.typeLabel,
                        .color = theme.colorTextPrimary,
                    },
                    Spacer{},
                    Text{
                        .text = valueText,
                        .style = theme.typeLabel,
                        .color = theme.colorTextSecondary,
                    }
                ),
            }.flex(1.f),
            Slider{
                .value = value,
                .min = min,
                .max = max,
                .step = step,
                .style = Slider::Style {
                    .activeColor = activeColor,
                },
            }.flex(1.f)
        ),
    };
  }
};

struct RgbColorSelectorRoot {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();

    auto red = useState(90.f);
    auto green = useState(120.f);
    auto blue = useState(200.f);

    Color const preview = Color{*red * kChannelScale, *green * kChannelScale, *blue * kChannelScale,
                                1.f};

    return ScrollView {
        .children = children(
            VStack {
                .spacing = 20.f,
                .alignment = Alignment::Start,
                .children = children(
                    Text{
                        .text = "RGB color",
                        .style = theme.typeDisplay,
                        .color = theme.colorTextPrimary,
                    },
                    Text{
                        .text = "Adjust red, green, and blue channels (0–255). The preview updates "
                                "as you drag or use the keyboard.",
                        .style = theme.typeBody,
                        .color = theme.colorTextSecondary,
                        .wrapping = TextWrapping::Wrap,
                    }.flex(1.f),

                    ZStack{
                        .horizontalAlignment = Alignment::Center,
                        .verticalAlignment = Alignment::Center,
                        .children = children(
                            Rectangle{}
                                .fill(FillStyle::solid(preview))
                                .stroke(StrokeStyle::solid(theme.colorBorder, 1.f))
                                .height(160.f)
                                .cornerRadius(CornerRadius{theme.radiusLarge})
                                .flex(1.f),
                            Text{
                                .text = fmtHex(*red, *green, *blue),
                                .style = theme.typeTitle,
                                .color = luminance(preview) > 0.55f ? theme.colorTextPrimary : theme.colorOnAccent,
                            }
                        ),
                    },

                    Text{
                        .text = fmtRgbLine(*red, *green, *blue),
                        .style = theme.typeBodySmall,
                        .color = theme.colorTextSecondary,
                    },

                    Text{
                        .text = "Channels",
                        .style = theme.typeHeading,
                        .color = theme.colorTextPrimary,
                    },
                    Element{LabeledSlider{
                        .label = "Red",
                        .valueText = fmtInt(*red),
                        .value = red,
                        .activeColor = theme.colorDanger,
                    }}.flex(1.f),
                    Element{LabeledSlider{
                        .label = "Green",
                        .valueText = fmtInt(*green),
                        .value = green,
                        .activeColor = theme.colorSuccess,
                    }}.flex(1.f),
                    Element{LabeledSlider{
                        .label = "Blue",
                        .valueText = fmtInt(*blue),
                        .value = blue,
                        .activeColor = theme.colorAccent,
                    }}.flex(1.f)
                ),
            }.padding(24.f)
        ),
    };
  }

  /// Relative luminance (sRGB), for readable overlay text on the swatch.
  static float luminance(Color const& c) {
    return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {480, 640},
      .title = "Flux — RGB color",
      .resizable = true,
  });
  w.setView<RgbColorSelectorRoot>();
  return app.exec();
}
