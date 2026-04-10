#include <Flux.hpp>
#include <Flux/Core/Action.hpp>
#include <Flux/Core/Shortcut.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Button.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Slider.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/TextArea.hpp>
#include <Flux/UI/Views/Toggle.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <cstdio>
#include <string>

using namespace flux;

namespace {

TextWrapping wrappingFromIndex(int i) {
  switch (i) {
  case 0:
    return TextWrapping::NoWrap;
  case 2:
    return TextWrapping::WrapAnywhere;
  default:
    return TextWrapping::Wrap;
  }
}

char const* wrappingLabel(int i) {
  switch (i) {
  case 0:
    return "No wrap";
  case 2:
    return "Anywhere";
  default:
    return "Word wrap";
  }
}

char const* fontPresetLabel(int p) {
  switch (p) {
  case 1:
    return "Body";
  case 2:
    return "Title";
  case 3:
    return "Code";
  default:
    return "Theme";
  }
}

} // namespace

struct Divider : ViewModifiers<Divider> {
  enum class Orientation {
    Horizontal,
    Vertical
  };

  Orientation orientation = Orientation::Horizontal;

  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    return Rectangle{}
        .size(orientation == Orientation::Horizontal ? 0.f : 1.f,
               orientation == Orientation::Horizontal ? 1.f : 0.f)
        .cornerRadius(1.f)
        .fill(FillStyle::solid(theme.colorBorder));
  }
};

struct LabeledSliderRow {
  std::string label;
  std::string valueText;
  State<float> value{};
  float min = 0.f;
  float max = 1.f;
  float step = 1.f;

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
                        .style = theme.typeLabelSmall,
                        .color = theme.colorTextSecondary,
                    }
                ),
            },
            Slider{
                .value = value,
                .min = min,
                .max = max,
                .step = step,
            }
        ),
    };
  }
};

struct TextAreaDemo {
  auto body() const {
    auto theme = useEnvironment<Theme>();

    auto text = useState(std::string{
        R"(Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec dictum mi et metus porta malesuada. Nullam vitae urna orci. Sed sit amet vulputate erat, vel molestie velit. Etiam mollis dui non vehicula sagittis. Sed mollis tellus eget magna blandit congue. Nulla varius ipsum at eros congue, posuere dignissim tortor tincidunt. Duis sollicitudin at felis non pretium. Orci varius natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Phasellus efficitur blandit lacus, non porta orci feugiat non. Maecenas pharetra ultrices ante ut tristique. Vestibulum mattis diam et ultrices faucibus. Mauris cursus orci vel arcu blandit bibendum. Interdum et malesuada fames ac ante ipsum primis in faucibus.

In facilisis odio sit amet pulvinar fringilla. Aliquam accumsan, urna dictum scelerisque lobortis, eros turpis volutpat sapien, et commodo nisl sapien sed ipsum. Vivamus molestie in felis eget pretium. Vestibulum eleifend ac leo sit amet mattis. Vivamus condimentum tincidunt faucibus. Etiam a auctor tellus. Morbi porta, magna sit amet fringilla suscipit, massa lorem vulputate nulla, sit amet convallis orci libero porta mi. Phasellus ut est eget purus lobortis molestie sed sit amet magna. Ut rutrum elementum nisi, sollicitudin tempor ex luctus id. Nunc consequat sapien non nisi viverra, in consectetur est cursus. In in sapien id tortor tristique porttitor sed a arcu. Nulla vestibulum elit vel commodo pretium. Cras in risus sed nisl vestibulum accumsan. Morbi quam ex, sodales sit amet felis vitae, ornare tincidunt ipsum. Sed vitae augue vel tellus volutpat pellentesque. Vivamus sit amet malesuada diam, vel fermentum enim.

Etiam quam odio, consequat id urna id, lobortis elementum nibh. Quisque tristique mi vel lobortis ornare. Vivamus in ornare orci. Nullam in tellus quis nulla tincidunt consectetur sit amet molestie justo. Ut volutpat tortor sapien, quis tincidunt nisl ullamcorper eget. Aenean quis commodo massa, sit amet rhoncus augue. Nullam ac felis a diam pretium efficitur quis et erat.

Cras nisi quam, suscipit a egestas eget, lacinia quis nisl. Phasellus eu urna dapibus purus fringilla lobortis. Proin sapien tellus, aliquet ultricies nisi sed, hendrerit semper arcu. Fusce vitae ultricies enim. Curabitur mattis elit vestibulum, rutrum lacus et, dictum ante. Phasellus in aliquet neque. Duis lobortis luctus posuere. Praesent eleifend vehicula ante, vitae luctus mi elementum in.

Etiam ornare neque quis ante porttitor sodales eget sit amet urna. Phasellus nec lacus eget lacus commodo viverra eget a ex. Nulla facilisi. In ornare pulvinar accumsan. Curabitur quis ullamcorper leo. Duis fermentum elementum est. Sed enim ipsum, tincidunt mollis odio ut, venenatis pulvinar nibh. Nulla aliquet nisi elit, vel auctor massa tempus in. Praesent tempus, mauris id efficitur fringilla, quam urna mollis sem, ut aliquet ipsum sapien vel massa. In hac habitasse platea dictumst. Nam commodo, quam sit amet scelerisque rhoncus, ante nibh vestibulum diam, id porta nibh velit eget lorem. Duis libero nulla, rhoncus ornare mattis eu, eleifend eu leo. Sed id magna arcu. Nulla facilisi. Vivamus consequat dolor et urna posuere, ut tempus augue sollicitudin. Aenean bibendum ut odio ut imperdiet.)"});

    auto wrapIndex = useState(1);
    auto disabled = useState(false);
    auto fontPreset = useState(0);
    auto borderWidth = useState(theme.textAreaBorderWidth);
    auto cornerRadius = useState(theme.textAreaCornerRadius);
    auto paddingUniform = useState(10.f);
    auto accentBorder = useState(false);
    auto warmBackground = useState(false);

    TextArea::Style areaStyle{};
    switch (*fontPreset) {
    case 1:
      areaStyle.font = theme.typeBody.toFont();
      break;
    case 2:
      areaStyle.font = theme.typeTitle.toFont();
      break;
    case 3:
      areaStyle.font = theme.typeCode.toFont();
      break;
    default:
      break;
    }
    areaStyle.borderWidth = *borderWidth;
    areaStyle.cornerRadius = *cornerRadius;
    areaStyle.padding = EdgeInsets::uniform(*paddingUniform);
    if (*accentBorder) {
      areaStyle.borderColor = theme.colorAccent;
    }
    if (*warmBackground) {
      areaStyle.backgroundColor = Color::rgb(255, 252, 245);
    }

    auto fmtBorder = [](float v) {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%.0f pt", static_cast<double>(v));
      return std::string(buf);
    };
    auto fmtRadius = [](float v) {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%.0f pt", static_cast<double>(v));
      return std::string(buf);
    };
    auto fmtPad = [](float v) {
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%.0f pt", static_cast<double>(v));
      return std::string(buf);
    };

    return VStack{
        .spacing = 16.f,
        .children =
            children(
                Text{
                    .text = "TextArea Demo",
                    .style = theme.typeDisplay,
                    .color = theme.colorTextPrimary,
                },
                Text{
                    .text = "Use the controls on the right to tweak wrapping, typography, chrome, and "
                            "disabled state.",
                    .style = theme.typeBody,
                    .color = theme.colorTextSecondary,
                    .wrapping = TextWrapping::Wrap,
                },
                HStack{
                    .spacing = 16.f,
                    .children =
                        children(
                            TextArea{
                                .value = text,
                                .style = areaStyle,
                                .placeholder = "Start typing…",
                                .wrapping = wrappingFromIndex(*wrapIndex),
                                .disabled = *disabled,
                            }
                                .flex(3.f, 3.f, 0.f),
                            ScrollView{
                                .axis = ScrollAxis::Vertical,
                                .children =
                                    children(
                                        VStack{
                                            .spacing = 12.f,
                                            .alignment = Alignment::Start,
                                            .children =
                                                children(
                                                    Text{
                                                        .text = "Properties",
                                                        .style = theme.typeLabel,
                                                        .color = theme.colorTextPrimary,
                                                    },
                                                    Divider{
                                                        .orientation = Divider::Orientation::Horizontal,
                                                    },
                                                    Text{
                                                        .text = "Wrapping",
                                                        .style = theme.typeLabelSmall,
                                                        .color = theme.colorTextSecondary,
                                                    },
                                                    HStack{
                                                        .spacing = 4.f,
                                                        .children =
                                                            children(
                                                                Button{
                                                                    .label = "No wrap",
                                                                    .variant = *wrapIndex == 0 ? ButtonVariant::Primary
                                                                                               : ButtonVariant::Secondary,
                                                                    .onTap = [wrapIndex] {
                                                                      wrapIndex = 0;
                                                                    },
                                                                },
                                                                Button{
                                                                    .label = "Word",
                                                                    .variant = *wrapIndex == 1 ? ButtonVariant::Primary
                                                                                               : ButtonVariant::Secondary,
                                                                    .onTap = [wrapIndex] {
                                                                      wrapIndex = 1;
                                                                    },
                                                                },
                                                                Button{
                                                                    .label = "Anywhere",
                                                                    .variant = *wrapIndex == 2 ? ButtonVariant::Primary
                                                                                               : ButtonVariant::Secondary,
                                                                    .onTap = [wrapIndex] {
                                                                      wrapIndex = 2;
                                                                    },
                                                                }
                                                            ),
                                                    },
                                                    Text{
                                                        .text = std::string("Current: ") + wrappingLabel(*wrapIndex),
                                                        .style = theme.typeBodySmall,
                                                        .color = theme.colorTextMuted,
                                                        .wrapping = TextWrapping::Wrap,
                                                    },
                                                    HStack{
                                                        .spacing = 12.f,
                                                        .alignment = Alignment::Center,
                                                        .children =
                                                            children(
                                                                Text{
                                                                    .text = "Disabled",
                                                                    .style = theme.typeBody,
                                                                    .color = *disabled ? theme.colorTextDisabled : theme.colorTextPrimary,
                                                                },
                                                                Spacer{},
                                                                Toggle{
                                                                    .value = disabled,
                                                                }
                                                            ),
                                                    },
                                                    Text{
                                                        .text = "Typography (font)",
                                                        .style = theme.typeLabelSmall,
                                                        .color = theme.colorTextSecondary,
                                                    },
                                                    HStack{
                                                        .spacing = 4.f,
                                                        .children =
                                                            children(
                                                                Button{
                                                                    .label = "Theme",
                                                                    .variant = *fontPreset == 0 ? ButtonVariant::Primary
                                                                                                : ButtonVariant::Secondary,
                                                                    .onTap = [fontPreset] {
                                                                      fontPreset = 0;
                                                                    },
                                                                },
                                                                Button{
                                                                    .label = "Body",
                                                                    .variant = *fontPreset == 1 ? ButtonVariant::Primary
                                                                                                : ButtonVariant::Secondary,
                                                                    .onTap = [fontPreset] {
                                                                      fontPreset = 1;
                                                                    },
                                                                }
                                                            ),
                                                    },
                                                    HStack{
                                                        .spacing = 4.f,
                                                        .children =
                                                            children(
                                                                Button{
                                                                    .label = "Title",
                                                                    .variant = *fontPreset == 2 ? ButtonVariant::Primary
                                                                                                : ButtonVariant::Secondary,
                                                                    .onTap = [fontPreset] {
                                                                      fontPreset = 2;
                                                                    },
                                                                },
                                                                Button{
                                                                    .label = "Code",
                                                                    .variant = *fontPreset == 3 ? ButtonVariant::Primary
                                                                                                : ButtonVariant::Secondary,
                                                                    .onTap = [fontPreset] {
                                                                      fontPreset = 3;
                                                                    },
                                                                }
                                                            ),
                                                    },
                                                    Text{
                                                        .text = std::string("Preset: ") + fontPresetLabel(*fontPreset),
                                                        .style = theme.typeBodySmall,
                                                        .color = theme.colorTextMuted,
                                                        .wrapping = TextWrapping::Wrap,
                                                    },
                                                    Text{
                                                        .text = "Style",
                                                        .style = theme.typeLabelSmall,
                                                        .color = theme.colorTextSecondary,
                                                    },
                                                    HStack{
                                                        .spacing = 12.f,
                                                        .alignment = Alignment::Center,
                                                        .children =
                                                            children(
                                                                Text{
                                                                    .text = "Accent border",
                                                                    .style = theme.typeBody,
                                                                    .color = theme.colorTextPrimary,
                                                                },
                                                                Spacer{},
                                                                Toggle{
                                                                    .value = accentBorder,
                                                                }
                                                            ),
                                                    },
                                                    HStack{
                                                        .spacing = 12.f,
                                                        .alignment = Alignment::Center,
                                                        .children =
                                                            children(
                                                                Text{
                                                                    .text = "Warm background",
                                                                    .style = theme.typeBody,
                                                                    .color = theme.colorTextPrimary,
                                                                },
                                                                Spacer{},
                                                                Toggle{
                                                                    .value = warmBackground,
                                                                }
                                                            ),
                                                    },
                                                    Element{LabeledSliderRow{
                                                        .label = "Border width",
                                                        .valueText = fmtBorder(*borderWidth),
                                                        .value = borderWidth,
                                                        .min = 0.f,
                                                        .max = 4.f,
                                                        .step = 0.5f,
                                                    }},
                                                    Element{LabeledSliderRow{
                                                        .label = "Corner radius",
                                                        .valueText = fmtRadius(*cornerRadius),
                                                        .value = cornerRadius,
                                                        .min = 0.f,
                                                        .max = 20.f,
                                                        .step = 1.f,
                                                    }},
                                                    Element{LabeledSliderRow{
                                                        .label = "Padding",
                                                        .valueText = fmtPad(*paddingUniform),
                                                        .value = paddingUniform,
                                                        .min = 4.f,
                                                        .max = 28.f,
                                                        .step = 1.f,
                                                    }}
                                                ),
                                        }
                                    ),
                            }
                                .flex(1.f, 1.f, 180.f)),
                }
                    .flex(1.f, 1.f, 0.f)),
    }
        .padding(16.f);
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {720, 560},
      .title = "Flux — TextArea Demo",
      .resizable = true,
  });

  w.setView<TextAreaDemo>();
  return app.exec();
}
