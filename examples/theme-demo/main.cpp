// Demonstrates Theme, useEnvironment, Element::environment(), and window-level theme switching.
#include <Flux.hpp>

#include <string>
#include <Flux/Core/Application.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Button.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

using namespace flux;

Window* gThemeDemoWindow = nullptr;

namespace {

Theme themeForPreset(int preset) {
  switch (preset) {
  case 0:
    return Theme::light();
  case 1:
    return Theme::dark();
  case 2:
    return Theme::compact();
  case 3:
    return Theme::comfortable();
  default:
    return Theme::light();
  }
}

char const* presetLabel(int preset) {
  switch (preset) {
  case 0:
    return "Light";
  case 1:
    return "Dark";
  case 2:
    return "Compact";
  case 3:
    return "Comfortable";
  default:
    return "Light";
  }
}

} // namespace

struct ThemeDemoRoot {
  auto body() const {
    auto windowPreset = useState(0);

    Theme const& windowTheme = useEnvironment<Theme>();

    auto pane = [&](char const* title, char const* subtitle, Theme const& t) {
      return VStack{
          .spacing = windowTheme.space2,
          .alignment = Alignment::Start,
          .children = children(
                  Text{ .text = title,
                        .font = t.fontTitle,
                        .color = t.colorTextPrimary },
                  HStack{
                      .spacing = 0.f,
                      .children = children(
                              Text{
                                      .text = subtitle,
                                      .font = t.fontBody,
                                      .color = t.colorTextSecondary,
                                      .wrapping = TextWrapping::Wrap,
                                  }
                                  .flex(1.f)
                          ),
                  },
                  Button{ .label = "Accent",
                          .variant = ButtonVariant::Primary,
                          .onTap = [] {} }
              ),
      }.padding(windowTheme.space4);
    };

    return ZStack{
        .children = children(
                Rectangle{}.fill(FillStyle::solid(windowTheme.colorBackground)),
                VStack{
                    .spacing = windowTheme.space4,
                    .alignment = Alignment::Start,
                    .children = children(
                            Text{ .text = "Theme & environment",
                                  .font = windowTheme.fontDisplay,
                                  .color = windowTheme.colorTextPrimary },
                            HStack{
                                .spacing = 0.f,
                                .children = children(
                                        Text{
                                                .text = "Window theme drives defaults. The right column uses "
                                                        "Element::environment(Theme::dark()) for a subtree override.",
                                                .font = windowTheme.fontBody,
                                                .color = windowTheme.colorTextSecondary,
                                                .wrapping = TextWrapping::Wrap,
                                            }
                                            .flex(1.f)
                                    ),
                            },
                            HStack{
                                .spacing = 0.f,
                                .children = children(
                                        Text{
                                                .text = std::string("Window preset: ") + presetLabel(*windowPreset) +
                                                        "  (density " + std::to_string(windowTheme.density) +
                                                        ", horizontal spacing token space4=" +
                                                        std::to_string(windowTheme.space4) + "pt)",
                                                .font = windowTheme.fontBodySmall,
                                                .color = windowTheme.colorTextMuted,
                                                .wrapping = TextWrapping::Wrap,
                                            }
                                            .flex(1.f)
                                    ),
                            },
                            HStack{
                                .spacing = windowTheme.space2,
                                .alignment = Alignment::Center,
                                .children = children(
                                        Button{
                                            .label = "Light",
                                            .variant = *windowPreset == 0 ? ButtonVariant::Primary
                                                                          : ButtonVariant::Secondary,
                                            .onTap = [windowPreset] {
                                              if (!gThemeDemoWindow) {
                                                return;
                                              }
                                              gThemeDemoWindow->setEnvironmentValue(themeForPreset(0));
                                              windowPreset = 0;
                                              Application::instance().markReactiveDirty();
                                            },
                                        },
                                        Button{
                                            .label = "Dark",
                                            .variant = *windowPreset == 1 ? ButtonVariant::Primary
                                                                          : ButtonVariant::Secondary,
                                            .onTap = [windowPreset] {
                                              if (!gThemeDemoWindow) {
                                                return;
                                              }
                                              gThemeDemoWindow->setEnvironmentValue(themeForPreset(1));
                                              windowPreset = 1;
                                              Application::instance().markReactiveDirty();
                                            },
                                        },
                                        Button{
                                            .label = "Compact",
                                            .variant = *windowPreset == 2 ? ButtonVariant::Primary
                                                                          : ButtonVariant::Secondary,
                                            .onTap = [windowPreset] {
                                              if (!gThemeDemoWindow) {
                                                return;
                                              }
                                              gThemeDemoWindow->setEnvironmentValue(themeForPreset(2));
                                              windowPreset = 2;
                                              Application::instance().markReactiveDirty();
                                            },
                                        },
                                        Button{
                                            .label = "Comfortable",
                                            .variant = *windowPreset == 3 ? ButtonVariant::Primary
                                                                          : ButtonVariant::Secondary,
                                            .onTap = [windowPreset] {
                                              if (!gThemeDemoWindow) {
                                                return;
                                              }
                                              gThemeDemoWindow->setEnvironmentValue(themeForPreset(3));
                                              windowPreset = 3;
                                              Application::instance().markReactiveDirty();
                                            },
                                        }
                                    ),
                            },
                            HStack{
                                .spacing = windowTheme.space3,
                                .alignment = Alignment::Start,
                                .children = children(
                                        pane("Light pane",
                                             "Subtree explicitly set to Theme::light().",
                                             Theme::light())
                                            .environment(Theme::light()),
                                        pane("Dark pane",
                                             "Subtree uses Theme::dark() tokens.",
                                             Theme::dark())
                                            .environment(Theme::dark())
                                    ),
                            }
                    ),
                }.padding(windowTheme.space5)
            ),
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {640, 480},
      .title = "Flux — Theme demo",
      .resizable = true,
  });
  gThemeDemoWindow = &w;
  w.setView<ThemeDemoRoot>();
  return app.exec();
}
