// Demonstrates Button: variants, disabled state, Tab focus, Link inline text, and window actions.
#include <Flux.hpp>
#include <Flux/Core/Action.hpp>
#include <Flux/Core/Shortcut.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Button.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <cstdio>

using namespace flux;

namespace {

bool gSaveActionEnabled = true;

} // namespace

struct ButtonDemoRoot {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    return ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children = children(
                Rectangle{ .fill = FillStyle::solid(theme.colorBackground) },
                VStack{
                    .spacing = 20.f,
                    .hAlign = HorizontalAlignment::Leading,
                    .children = children(
                            Text{ .text = "Button",
                                  .style = theme.typeDisplay,
                                  .color = theme.colorTextPrimary },
                            HStack{
                                .spacing = 0.f,
                                .children = children(
                                        Text{
                                                .text = "Five variants, hover/press/focus, disabled state, and Tab navigation. "
                                                        "Link focus rings appear only after keyboard focus.",
                                                .style = theme.typeBody,
                                                .color = theme.colorTextSecondary,
                                                .wrapping = TextWrapping::Wrap,
                                            }
                                            .flex(1.f)
                                    ),
                            },
                            Text{ .text = "Variants",
                                  .style = theme.typeHeading,
                                  .color = theme.colorTextPrimary },
                            HStack{
                                .spacing = 8.f,
                                .vAlign = VerticalAlignment::Center,
                                .children = children(
                                        Button{ .label = "Primary",
                                                .variant = ButtonVariant::Primary,
                                                .onTap = [] {
                                                  std::fprintf(stderr, "[button-demo] Primary\n");
                                                } },
                                        Button{ .label = "Secondary",
                                                .variant = ButtonVariant::Secondary,
                                                .onTap = [] {
                                                  std::fprintf(stderr, "[button-demo] Secondary\n");
                                                } },
                                        Button{ .label = "Destructive",
                                                .variant = ButtonVariant::Destructive,
                                                .onTap = [] {
                                                  std::fprintf(stderr, "[button-demo] Destructive\n");
                                                } },
                                        Button{ .label = "Ghost",
                                                .variant = ButtonVariant::Ghost,
                                                .onTap = [] {
                                                  std::fprintf(stderr, "[button-demo] Ghost\n");
                                                } }
                                    ),
                            },
                            Text{ .text = "Disabled",
                                  .style = theme.typeHeading,
                                  .color = theme.colorTextPrimary },
                            HStack{
                                .spacing = 8.f,
                                .vAlign = VerticalAlignment::Center,
                                .children = children(
                                        Button{ .label = "Primary",
                                                .variant = ButtonVariant::Primary,
                                                .disabled = true },
                                        Button{ .label = "Secondary",
                                                .variant = ButtonVariant::Secondary,
                                                .disabled = true },
                                        Button{ .label = "Destructive",
                                                .variant = ButtonVariant::Destructive,
                                                .disabled = true },
                                        Button{ .label = "Ghost",
                                                .variant = ButtonVariant::Ghost,
                                                .disabled = true }
                                    ),
                            },
                            HStack{
                                .spacing = 4.f,
                                .vAlign = VerticalAlignment::Center,
                                .children = children(
                                        Text{ .text = "Inline link —",
                                              .style = theme.typeBodySmall,
                                              .color = theme.colorTextSecondary },
                                        Button{ .label = "Forgot password?",
                                                .variant = ButtonVariant::Link,
                                                .font = theme.typeBodySmall.toFont(),
                                                .onTap = [] {
                                                  std::fprintf(stderr, "[button-demo] Link tap\n");
                                                } }
                                    ),
                            },
                            Text{ .text = "Form (Tab between controls; Cmd+S when Save is enabled)",
                                  .style = theme.typeHeading,
                                  .color = theme.colorTextPrimary },
                            HStack{
                                .spacing = 8.f,
                                .vAlign = VerticalAlignment::Center,
                                .children = children(
                                        Button{
                                            .label = "Toggle save action",
                                            .variant = ButtonVariant::Ghost,
                                            .onTap = [] {
                                              gSaveActionEnabled = !gSaveActionEnabled;
                                              Application::instance().markReactiveDirty();
                                            },
                                        },
                                        Spacer{},
                                        Button{
                                            .label = "Cancel",
                                            .variant = ButtonVariant::Secondary,
                                            .onTap = [] {
                                              std::fprintf(stderr, "[button-demo] Cancel\n");
                                            },
                                        },
                                        Button{
                                            .label = "Save",
                                            .variant = ButtonVariant::Primary,
                                            .onTap = [] {
                                              std::fprintf(stderr, "[button-demo] Save (button or shortcut)\n");
                                            },
                                            .actionName = "demo.save",
                                        }
                                    ),
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
      .size = { 640, 720 },
      .title = "Flux — Button demo",
      .resizable = true,
  });

  w.registerAction("demo.save",
                   {
                       .label = "Save",
                       .shortcut = shortcuts::Save,
                       .isEnabled = [] { return gSaveActionEnabled; },
                   });

  w.setView<ButtonDemoRoot>();
  return app.exec();
}
