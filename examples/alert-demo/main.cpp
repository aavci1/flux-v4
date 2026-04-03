// Demonstrates Alert + useAlert: informational, confirmation, three-button, Escape, disabled.
#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Alert.hpp>
#include <Flux/UI/Views/Button.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>

#include <string>

using namespace flux;

struct AlertDemoRoot {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    auto [showAlert, hideAlert, alertOpen] = useAlert();
    (void)alertOpen;

    auto filename = useState(std::string{"report.pdf"});
    auto status = useState(std::string{"Tap a button to open an alert."});

    return ZStack{
        .children = children(
                Rectangle{.fill = FillStyle::solid(theme.colorBackground)},
                VStack{
                    .spacing = 16.f,
                    .hAlign = HorizontalAlignment::Leading,
                    .children = children(
                            Text{.text = "Alert demo",
                                 .style = theme.typeDisplay,
                                 .color = theme.colorTextPrimary},
                            Text{.text = "Modal alerts via useAlert(). Escape dismisses when enabled. "
                                         "Outside tap does not dismiss.",
                                 .style = theme.typeBody,
                                 .color = theme.colorTextSecondary,
                                 .wrapping = TextWrapping::Wrap},
                            Text{.text = *status,
                                 .style = theme.typeBodySmall,
                                 .color = theme.colorAccent,
                                 .wrapping = TextWrapping::Wrap},

                            Button{
                                .label = "Delete file (confirmation)",
                                .variant = ButtonVariant::Destructive,
                                .onTap =
                                    [=] {
                                      showAlert(Alert{
                                          .title = "Delete \"" + *filename + "\"?",
                                          .message = "This file will be permanently removed and cannot be recovered.",
                                          .buttons =
                                              {
                                                  {.label = "Cancel",
                                                   .variant = ButtonVariant::Secondary,
                                                   .action = hideAlert},
                                                  {.label = "Delete",
                                                   .variant = ButtonVariant::Destructive,
                                                   .action =
                                                       [=] {
                                                         status = std::string{"Deleted (simulated)."};
                                                       }},
                                              },
                                      });
                                    },
                            },

                            Button{
                                .label = "Show info (single OK)",
                                .variant = ButtonVariant::Secondary,
                                .onTap =
                                    [=] {
                                      showAlert(Alert{
                                          .title = "Upload complete",
                                          .message = "\"" + *filename + "\" was uploaded successfully.",
                                      });
                                    },
                            },

                            Button{
                                .label = "Close document (three buttons)",
                                .variant = ButtonVariant::Ghost,
                                .onTap =
                                    [=] {
                                      showAlert(Alert{
                                          .title = "Save changes?",
                                          .message = "Your changes will be lost if you don't save.",
                                          .buttons =
                                              {
                                                  {.label = "Cancel",
                                                   .variant = ButtonVariant::Secondary,
                                                   .action = hideAlert},
                                                  {.label = "Don't Save",
                                                   .variant = ButtonVariant::Ghost,
                                                   .action =
                                                       [=] {
                                                         status = std::string{"Closed without saving."};
                                                       }},
                                                  {.label = "Save",
                                                   .variant = ButtonVariant::Primary,
                                                   .action =
                                                       [=] {
                                                         status = std::string{"Saved and closed."};
                                                       }},
                                              },
                                      });
                                    },
                            },

                            Button{
                                .label = "Alert with disabled action",
                                .variant = ButtonVariant::Secondary,
                                .onTap =
                                    [=] {
                                      showAlert(Alert{
                                          .title = "Requires upgrade",
                                          .message = "This action is not available on your plan.",
                                          .buttons =
                                              {
                                                  {.label = "Cancel", .variant = ButtonVariant::Secondary, .action = hideAlert},
                                                  {.label = "Upgrade",
                                                   .variant = ButtonVariant::Primary,
                                                   .disabled = true,
                                                   .action =
                                                       [=] {
                                                         status = std::string{"Should not run while disabled."};
                                                       }},
                                              },
                                      });
                                    },
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
      .size = {520, 520},
      .title = "Flux — Alert demo",
      .resizable = true,
  });
  w.setView<AlertDemoRoot>();
  return app.exec();
}
