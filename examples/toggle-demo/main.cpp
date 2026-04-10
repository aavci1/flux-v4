// Demonstrates Toggle: on/off, disabled, custom colors, sizing, labeled rows.
#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/Toggle.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <cstdio>
#include <string>

using namespace flux;

struct LabeledToggle {
  std::string label;
  State<bool> value{};
  bool disabled = false;

  auto body() const {
    Theme const& theme = useEnvironment<Theme>();

    return HStack{
        .spacing = 12.f,
        .alignment = Alignment::Center,
        .children = children(
            Text{
                .text = label,
                .font = theme.fontBody,
                .color = disabled ? theme.colorTextDisabled : theme.colorTextPrimary,
            },
            Spacer{},
            Toggle{
                .value = value,
                .disabled = disabled,
                .onChange = [label = label](bool v) {
                  std::fprintf(stderr, "[toggle-demo] %s → %s\n", label.c_str(), v ? "ON" : "OFF");
                },
            }
        ),
    };
  }
};

struct ToggleDemoRoot {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();

    auto wifiEnabled = useState(true);
    auto bluetoothEnabled = useState(false);
    auto darkMode = useState(false);
    auto notifications = useState(false);
    auto customColor = useState(true);
    auto smallToggle = useState(false);

    std::string statusText =
        std::string("Wi-Fi: ") + (*wifiEnabled ? "on" : "off") + " · Bluetooth: " +
        (*bluetoothEnabled ? "on" : "off") + " · Dark mode: " + (*darkMode ? "on" : "off") +
        " · Notifications: " + (*notifications ? "on" : "off");

    return ScrollView{
        .axis = ScrollAxis::Vertical,
        .children = children(
            VStack{
                .spacing = 20.f,
                .alignment = Alignment::Start,
                .children = children(
                    Text{
                        .text = "Toggle",
                        .font = theme.fontDisplay,
                        .color = theme.colorTextPrimary,
                    },
                    Text{
                        .text = "Boolean switch with animated thumb, "
                                "focus ring, disabled state, and "
                                "custom sizing.",
                        .font = theme.fontBody,
                        .color = theme.colorTextSecondary,
                        .wrapping = TextWrapping::Wrap,
                    }.flex(1.f),

                    Text{
                        .text = "Settings",
                        .font = theme.fontHeading,
                        .color = theme.colorTextPrimary,
                    },
                    Element{LabeledToggle{
                        .label = "Wi-Fi",
                        .value = wifiEnabled,
                    }}.flex(1.f),
                    Element{LabeledToggle{
                        .label = "Bluetooth",
                        .value = bluetoothEnabled,
                    }}.flex(1.f),
                    Element{LabeledToggle{
                        .label = "Dark mode",
                        .value = darkMode,
                    }}.flex(1.f),
                    Element{LabeledToggle{
                        .label = "Notifications (disabled)",
                        .value = notifications,
                        .disabled = true,
                    }}.flex(1.f),

                    Text{
                        .text = "Custom colors",
                        .font = theme.fontHeading,
                        .color = theme.colorTextPrimary,
                    },
                    HStack{
                        .spacing = 12.f,
                        .alignment = Alignment::Center,
                        .children = children(
                            Text{
                                .text = "Green accent",
                                .font = theme.fontBody,
                                .color = theme.colorTextPrimary,
                            },
                            Spacer{},
                            Toggle{
                                .value = customColor,
                                .style = Toggle::Style {
                                    .onColor = theme.colorSuccess,
                                },
                            }
                        ),
                    },

                    Text{
                        .text = "Custom sizing",
                        .font = theme.fontHeading,
                        .color = theme.colorTextPrimary,
                    },
                    HStack{
                        .spacing = 12.f,
                        .alignment = Alignment::Center,
                        .children = children(
                            Text{
                                .text = "Compact (34 × 20)",
                                .font = theme.fontBody,
                                .color = theme.colorTextPrimary,
                            },
                            Spacer{},
                            Toggle{
                                .value = smallToggle,
                                .style = Toggle::Style {
                                    .trackWidth = 34.f,
                                    .trackHeight = 20.f,
                                    .thumbInset = 2.f,
                                },
                            }
                        ),
                    },

                    Text{
                        .text = statusText,
                        .font = theme.fontBodySmall,
                        .color = theme.colorTextMuted,
                        .wrapping = TextWrapping::Wrap,
                    }
                ),
            }.padding(24.f)
        ),
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {480, 640},
      .title = "Flux — Toggle demo",
      .resizable = true,
  });
  w.setView<ToggleDemoRoot>();
  return app.exec();
}
