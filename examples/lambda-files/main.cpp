#include "FilesApp.hpp"

#include <Flux.hpp>
#include <Flux/UI/Application.hpp>
#include <Flux/UI/Window.hpp>

int main(int argc, char* argv[]) {
  flux::Application app(argc, argv);
  app.setName("lambda-files");

  auto& window = app.createWindow<flux::Window>({
      .size = {1040.f, 680.f},
      .title = "Files",
      .titlebar = flux::WindowTitlebarMode::Integrated,
      .resizable = true,
  });
  window.setBackground(flux::WindowBackground::glassEffect());
  window.setView<lambda_files::FilesAppRoot>({.window = &window});

  return app.exec();
}
