// Demonstrates Checkbox: checked, unchecked, indeterminate, disabled,
// custom colors, labeled rows, and a select-all pattern.
#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Checkbox.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <cstdio>
#include <string>

using namespace flux;

// ── Reusable labeled checkbox ──────────────────────────────────────────────

struct LabeledCheckbox {
  std::string label;
  State<bool> value{};
  bool indeterminate = false;
  bool disabled = false;

  auto body() const {
    FluxTheme const& theme = useEnvironment<FluxTheme>();

    return HStack{
        .spacing = 10.f,
        .vAlign = VerticalAlignment::Center,
        .children =
            {
                Checkbox{
                    .value = value,
                    .indeterminate = indeterminate,
                    .disabled = disabled,
                    .onChange = [label = label](bool v) {
                      std::fprintf(stderr, "[checkbox-demo] %s → %s\n", label.c_str(),
                                   v ? "checked" : "unchecked");
                    },
                },
                Text{
                    .text = label,
                    .font = theme.typeBody.toFont(),
                    .color = disabled ? theme.colorTextDisabled : theme.colorTextPrimary,
                },
            },
    };
  }
};

// ── Root view ──────────────────────────────────────────────────────────────

struct CheckboxDemoRoot {
  auto body() const {
    FluxTheme const& theme = useEnvironment<FluxTheme>();

    auto termsAccepted = useState(false);
    auto newsletter = useState(true);

    auto itemA = useState(true);
    auto itemB = useState(false);
    auto itemC = useState(true);

    bool const allChecked = *itemA && *itemB && *itemC;
    bool const noneChecked = !*itemA && !*itemB && !*itemC;
    bool const selectAllValue = allChecked;
    bool const selectAllIndeterminate = !allChecked && !noneChecked;

    auto selectAll = useState(selectAllValue);
    if (*selectAll != selectAllValue) {
      selectAll = selectAllValue;
    }

    auto greenCheck = useState(true);

    std::string status = std::string("Terms: ") + (*termsAccepted ? "yes" : "no") +
                         " · Newsletter: " + (*newsletter ? "yes" : "no") + " · Items: " +
                         (*itemA ? "A" : "") + (*itemB ? "B" : "") + (*itemC ? "C" : "");

    return ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children =
            {
                Rectangle{.fill = FillStyle::solid(theme.colorBackground)},
                VStack{
                    .spacing = 16.f,
                    .padding = 24.f,
                    .hAlign = HorizontalAlignment::Leading,
                    .children =
                        {
                            Text{
                                .text = "Checkbox",
                                .font = theme.typeHeading.toFont(),
                                .color = theme.colorTextPrimary,
                            },
                            Text{
                                        .text = "Boolean check with animated icon, "
                                                "indeterminate state, focus ring, "
                                                "and press scale.",
                                        .font = theme.typeBody.toFont(),
                                        .color = theme.colorTextSecondary,
                                        .wrapping = TextWrapping::Wrap,
                                    }
                                .withFlex(1.f),

                            Text{
                                .text = "Form controls",
                                .font = theme.typeSubtitle.toFont(),
                                .color = theme.colorTextPrimary,
                            },
                            Element{LabeledCheckbox{
                                        .label = "I accept the terms and conditions",
                                        .value = termsAccepted,
                                    }}
                                .withFlex(1.f),
                            Element{LabeledCheckbox{
                                        .label = "Subscribe to newsletter",
                                        .value = newsletter,
                                    }}
                                .withFlex(1.f),

                            Text{
                                .text = "Disabled",
                                .font = theme.typeSubtitle.toFont(),
                                .color = theme.colorTextPrimary,
                            },
                            Element{LabeledCheckbox{
                                        .label = "Checked (disabled)",
                                        .value = newsletter,
                                        .disabled = true,
                                    }}
                                .withFlex(1.f),
                            Element{LabeledCheckbox{
                                        .label = "Unchecked (disabled)",
                                        .value = termsAccepted,
                                        .disabled = true,
                                    }}
                                .withFlex(1.f),

                            Text{
                                .text = "Select all (indeterminate)",
                                .font = theme.typeSubtitle.toFont(),
                                .color = theme.colorTextPrimary,
                            },
                            HStack{
                                        .spacing = 10.f,
                                        .vAlign = VerticalAlignment::Center,
                                        .children =
                                            {
                                                Checkbox{
                                                    .value = selectAll,
                                                    .indeterminate = selectAllIndeterminate,
                                                    .onChange = [itemA, itemB, itemC](bool v) {
                                                      itemA = v;
                                                      itemB = v;
                                                      itemC = v;
                                                    },
                                                },
                                                Text{
                                                    .text = "Select all items",
                                                    .font = {.size = 15.f, .weight = 500.f},
                                                    .color = theme.colorTextPrimary,
                                                },
                                            },
                                    }
                                .withFlex(1.f),
                            VStack{
                                        .spacing = 8.f,
                                        .padding = 0.f,
                                        .children =
                                            {
                                                HStack{
                                                            .spacing = 10.f,
                                                            .children =
                                                                {
                                                                    Rectangle{
                                                                        .offsetX = 0.f, .offsetY = 0.f, .width = 20.f, .height = 0.f,
                                                                    },
                                                                    Element{LabeledCheckbox{
                                                                                .label = "Item A",
                                                                                .value = itemA,
                                                                            }}
                                                                        .withFlex(1.f),
                                                                },
                                                        }
                                                    .withFlex(1.f),
                                                HStack{
                                                            .spacing = 10.f,
                                                            .children =
                                                                {
                                                                    Rectangle{
                                                                        .offsetX = 0.f, .offsetY = 0.f, .width = 20.f, .height = 0.f,
                                                                    },
                                                                    Element{LabeledCheckbox{
                                                                                .label = "Item B",
                                                                                .value = itemB,
                                                                            }}
                                                                        .withFlex(1.f),
                                                                },
                                                        }
                                                    .withFlex(1.f),
                                                HStack{
                                                            .spacing = 10.f,
                                                            .children =
                                                                {
                                                                    Rectangle{
                                                                        .offsetX = 0.f, .offsetY = 0.f, .width = 20.f, .height = 0.f,
                                                                    },
                                                                    Element{LabeledCheckbox{
                                                                                .label = "Item C",
                                                                                .value = itemC,
                                                                            }}
                                                                        .withFlex(1.f),
                                                                },
                                                        }
                                                    .withFlex(1.f),
                                            },
                                    }
                                .withFlex(1.f),

                            Text{
                                .text = "Custom color",
                                .font = theme.typeSubtitle.toFont(),
                                .color = theme.colorTextPrimary,
                            },
                            HStack{
                                        .spacing = 10.f,
                                        .vAlign = VerticalAlignment::Center,
                                        .children =
                                            {
                                                Checkbox{
                                                    .value = greenCheck,
                                                    .style = Checkbox::Style {
                                                        .checkedColor = theme.colorSuccess,
                                                    },
                                                },
                                                Text{
                                                    .text = "Green accent",
                                                    .font = theme.typeBody.toFont(),
                                                    .color = theme.colorTextPrimary,
                                                },
                                            },
                                    }
                                .withFlex(1.f),

                            Text{
                                .text = status,
                                .font = theme.typeBodySmall.toFont(),
                                .color = theme.colorTextMuted,
                                .wrapping = TextWrapping::Wrap,
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
      .size = {480, 720},
      .title = "Flux — Checkbox demo",
      .resizable = true,
  });
  w.setView<CheckboxDemoRoot>();
  return app.exec();
}
