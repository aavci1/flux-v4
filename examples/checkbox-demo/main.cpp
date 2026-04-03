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
    Theme const& theme = useEnvironment<Theme>();

    return HStack{
        .spacing = 10.f,
        .vAlign = VerticalAlignment::Center,
        .children = children(
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
                    .style = theme.typeBody,
                    .color = disabled ? theme.colorTextDisabled : theme.colorTextPrimary,
                }
            ),
    };
  }
};

// ── Root view ──────────────────────────────────────────────────────────────

struct CheckboxDemoRoot {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();

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

    return
                VStack{
                    .spacing = 16.f,
                    .hAlign = HorizontalAlignment::Leading,
                    .children = children(
                            Text{
                                .text = "Checkbox",
                                .style = theme.typeDisplay,
                                .color = theme.colorTextPrimary,
                            },
                            Text{
                                        .text = "Boolean check with animated icon, "
                                                "indeterminate state, focus ring, "
                                                "and press scale.",
                                        .style = theme.typeBody,
                                        .color = theme.colorTextSecondary,
                                        .wrapping = TextWrapping::Wrap,
                                    }
                                .flex(1.f),

                            Text{
                                .text = "Form controls",
                                .style = theme.typeHeading,
                                .color = theme.colorTextPrimary,
                            },
                            Element{LabeledCheckbox{
                                        .label = "I accept the terms and conditions",
                                        .value = termsAccepted,
                                    }}
                                .flex(1.f),
                            Element{LabeledCheckbox{
                                        .label = "Subscribe to newsletter",
                                        .value = newsletter,
                                    }}
                                .flex(1.f),

                            Text{
                                .text = "Disabled",
                                .style = theme.typeHeading,
                                .color = theme.colorTextPrimary,
                            },
                            Element{LabeledCheckbox{
                                        .label = "Checked (disabled)",
                                        .value = newsletter,
                                        .disabled = true,
                                    }}
                                .flex(1.f),
                            Element{LabeledCheckbox{
                                        .label = "Unchecked (disabled)",
                                        .value = termsAccepted,
                                        .disabled = true,
                                    }}
                                .flex(1.f),

                            Text{
                                .text = "Select all (indeterminate)",
                                .style = theme.typeHeading,
                                .color = theme.colorTextPrimary,
                            },
                            HStack{
                                        .spacing = 10.f,
                                        .vAlign = VerticalAlignment::Center,
                                        .children = children(
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
                                                    .style = theme.typeLabel,
                                                    .color = theme.colorTextPrimary,
                                                }
                                            ),
                                    }
                                .flex(1.f),
                            VStack{
                                        .spacing = 8.f,
                                        .children = children(
                                                HStack{
                                                            .spacing = 10.f,
                                                            .children = children(
                                                                    Rectangle{}.width(20.f),
                                                                    Element{LabeledCheckbox{
                                                                                .label = "Item A",
                                                                                .value = itemA,
                                                                            }}
                                                                        .flex(1.f)
                                                                ),
                                                        }
                                                    .flex(1.f),
                                                HStack{
                                                            .spacing = 10.f,
                                                            .children = children(
                                                                    Rectangle{}.width(20.f),
                                                                    Element{LabeledCheckbox{
                                                                                .label = "Item B",
                                                                                .value = itemB,
                                                                            }}
                                                                        .flex(1.f)
                                                                ),
                                                        }
                                                    .flex(1.f),
                                                HStack{
                                                            .spacing = 10.f,
                                                            .children = children(
                                                                    Rectangle{}.width(20.f),
                                                                    Element{LabeledCheckbox{
                                                                                .label = "Item C",
                                                                                .value = itemC,
                                                                            }}
                                                                        .flex(1.f)
                                                                ),
                                                        }
                                                    .flex(1.f)
                                            ),
                                    }
                                .flex(1.f),

                            Text{
                                .text = "Custom color",
                                .style = theme.typeHeading,
                                .color = theme.colorTextPrimary,
                            },
                            HStack{
                                        .spacing = 10.f,
                                        .vAlign = VerticalAlignment::Center,
                                        .children = children(
                                                Checkbox{
                                                    .value = greenCheck,
                                                    .style = Checkbox::Style {
                                                        .checkedColor = theme.colorSuccess,
                                                    },
                                                },
                                                Text{
                                                    .text = "Green accent",
                                                    .style = theme.typeBody,
                                                    .color = theme.colorTextPrimary,
                                                }
                                            ),
                                    }
                                .flex(1.f),

                            Text{
                                .text = status,
                                .style = theme.typeBodySmall,
                                .color = theme.colorTextMuted,
                                .wrapping = TextWrapping::Wrap,
                            }
                        ),
                }.padding(24.f);
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
