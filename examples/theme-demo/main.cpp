// Demonstrates FluxTheme, useEnvironment, Element::environment(), and window-level theme switching.
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

FluxTheme themeForPreset(int preset) {
  switch (preset) {
  case 0:
    return FluxTheme::light();
  case 1:
    return FluxTheme::dark();
  case 2:
    return FluxTheme::compact();
  case 3:
    return FluxTheme::comfortable();
  default:
    return FluxTheme::light();
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

    FluxTheme const& windowTheme = useEnvironment<FluxTheme>();

    auto pane = [&](char const* title, char const* subtitle, FluxTheme const& t) {
      return VStack{
          .spacing = windowTheme.space2,
          .padding = windowTheme.space4,
          .hAlign = HorizontalAlignment::Leading,
          .children =
              {
                  Text{ .text = title,
                        .font = t.typeHeading.toFont(),
                        .color = t.colorTextPrimary,
                        .lineHeight = t.typeHeading.lineHeight },
                  Text{ .text = subtitle,
                        .font = t.typeBody.toFont(),
                        .color = t.colorTextSecondary,
                        .wrapping = TextWrapping::Wrap,
                        .lineHeight = t.typeBody.lineHeight,
                        .frame = { 0.f, 0.f, 200.f, 0.f } },
                  Button{ .label = "Accent",
                          .variant = ButtonVariant::Primary,
                          .onTap = [] {} },
              },
      };
    };

    return ZStack{
        .children =
            {
                Rectangle{ .fill = FillStyle::solid(windowTheme.colorBackground) },
                VStack{
                    .spacing = windowTheme.space4,
                    .padding = windowTheme.space5,
                    .hAlign = HorizontalAlignment::Leading,
                    .children =
                        {
                            Text{ .text = "Theme & environment",
                                  .font = windowTheme.typeHeading.toFont(),
                                  .color = windowTheme.colorTextPrimary,
                                  .lineHeight = windowTheme.typeHeading.lineHeight },
                            Text{
                                .text = "Window theme drives defaults. The right column uses "
                                        "Element::environment(FluxTheme::dark()) for a subtree override.",
                                .font = windowTheme.typeBody.toFont(),
                                .color = windowTheme.colorTextSecondary,
                                .wrapping = TextWrapping::Wrap,
                                .lineHeight = windowTheme.typeBody.lineHeight,
                                .frame = { 0.f, 0.f, 520.f, 0.f },
                            },
                            Text{
                                .text = std::string("Window preset: ") + presetLabel(*windowPreset) +
                                        "  (density " + std::to_string(windowTheme.density) +
                                        ", horizontal spacing token space4=" + std::to_string(windowTheme.space4) +
                                        "pt)",
                                .font = windowTheme.typeBodySmall.toFont(),
                                .color = windowTheme.colorTextMuted,
                                .wrapping = TextWrapping::Wrap,
                                .lineHeight = windowTheme.typeBodySmall.lineHeight,
                                .frame = { 0.f, 0.f, 520.f, 0.f },
                            },
                            HStack{
                                .spacing = windowTheme.space2,
                                .vAlign = VerticalAlignment::Center,
                                .children =
                                    {
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
                                        },
                                    },
                            },
                            HStack{
                                .spacing = windowTheme.space3,
                                .vAlign = VerticalAlignment::Top,
                                .children =
                                    {
                                        Element{ pane("Light pane",
                                                      "Subtree explicitly set to FluxTheme::light().",
                                                      FluxTheme::light()) }
                                            .environment(FluxTheme::light()),
                                        Element{ pane("Dark pane",
                                                      "Subtree uses FluxTheme::dark() tokens.",
                                                      FluxTheme::dark()) }
                                            .environment(FluxTheme::dark()),
                                    },
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
      .size = {640, 480},
      .title = "Flux — Theme demo",
      .resizable = true,
  });
  gThemeDemoWindow = &w;
  w.setView<ThemeDemoRoot>();
  return app.exec();
}
