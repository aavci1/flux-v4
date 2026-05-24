#include "FilesApp.hpp"

#include <Flux.hpp>
#include <Flux/UI/Application.hpp>
#include <Flux/UI/Window.hpp>

int main(int argc, char* argv[]) {
  flux::Application app(argc, argv);
  app.setName("files");

  auto& window = app.createWindow<flux::Window>({
      .size = {1040.f, 680.f},
      .title = "Files",
      .decorationMode = flux::WindowDecorationMode::IntegratedTitlebar,
      .resizable = true,
      .glass = {
          .enabled = true,
          .blurRadius = 46.f,
          .tint = {0.86f, 0.96f, 1.f, 0.56f},
          .borderColor = {1.f, 1.f, 1.f, 0.62f},
          .tintOpacity = 0.42f,
      },
  });
  if (window.platformCapabilities().supportsWindowGlass) {
    window.setClearColor(flux::Colors::transparent);
  }
  window.setView<lambda_files::FilesAppRoot>({.window = &window});

  return app.exec();
}
