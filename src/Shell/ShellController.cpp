#include "Shell/ShellController.hpp"

#include "Shell/ShellDesktopView.hpp"
#include "Shell/ShellJson.hpp"
#include "Shell/ShellViews.hpp"

#include <Flux/UI/EventQueue.hpp>
#include <Flux/UI/WindowUI.hpp>

#include <Flux/Core/Color.hpp>
#include <Flux/UI/Events.hpp>
#include <Flux/UI/KeyCodes.hpp>

#include <any>
#include <chrono>

namespace lambda_shell {

namespace {

using namespace flux;
using namespace flux::keys;

LayerShellOptions layerBase(LayerShellLayer layer, char const* nameSpace) {
  LayerShellOptions options{};
  options.enabled = true;
  options.layer = layer;
  options.nameSpace = nameSpace;
  options.backgroundBlur = true;
  return options;
}

} // namespace

flux::WindowConfig topBarWindowConfig() {
  LayerShellOptions layer = layerBase(LayerShellLayer::Top, "lambda.topbar");
  layer.anchorTop = true;
  layer.anchorLeft = true;
  layer.anchorRight = true;
  layer.exclusiveZone = kTopBarHeight;
  return WindowConfig{
      .size = {0.f, static_cast<float>(kTopBarHeight)},
      .title = "Lambda Top Bar",
      .resizable = false,
      .layerShell = layer,
  };
}

flux::WindowConfig dockWindowConfig(int width) {
  LayerShellOptions layer = layerBase(LayerShellLayer::Overlay, "lambda.dock");
  layer.anchorBottom = true;
  layer.marginBottom = kDockBottom;
  return WindowConfig{
      .size = {static_cast<float>(std::max(width, 1)), static_cast<float>(dockHeight())},
      .title = "Lambda Dock",
      .resizable = false,
      .layerShell = layer,
  };
}

flux::WindowConfig launcherWindowConfig() {
  LayerShellOptions layer = layerBase(LayerShellLayer::Overlay, "lambda.command-launcher");
  layer.anchorTop = true;
  layer.anchorBottom = true;
  layer.anchorLeft = true;
  layer.anchorRight = true;
  return WindowConfig{
      .size = {1.f, 1.f},
      .title = "Lambda Command Launcher",
      .resizable = false,
      .layerShell = layer,
  };
}

ShellController::ShellController(flux::Application& app, ShellModel& model) : app_(app), model_(model) {
  model_.resetDockItems();
  model_.setOnChanged([this] {
    if (previewHandle_) {
      remountPreviewView();
      requestRedraws();
      return;
    }
    remountTopBarView();
    remountDockView();
    if (model_.launcherOpen()) {
      remountLauncherView();
    }
    requestRedraws();
  });

  app_.eventQueue().on<flux::InputEvent>([this](flux::InputEvent const& event) {
    bool const forLauncher =
        (launcherHandle_ && event.handle == *launcherHandle_) || (previewHandle_ && event.handle == *previewHandle_);
    if (forLauncher && model_.launcherOpen()) {
      handleLauncherKey(event);
    }
  });

  app_.eventQueue().on<flux::TimerEvent>([this](flux::TimerEvent const& event) {
    if (event.timerId == clockTimerId_) {
      if (previewHandle_) {
        remountPreviewView();
      } else {
        remountTopBarView();
      }
      requestRedraws();
    }
  });

  app_.eventQueue().on<flux::WindowEvent>([this](flux::WindowEvent const& event) {
    if (event.kind != flux::WindowEvent::Kind::Resize) {
      return;
    }
    if (launcherHandle_ && event.handle == *launcherHandle_ && model_.launcherOpen()) {
      remountLauncherView();
      launcherWindow_->requestRedraw();
    }
  });

  app_.eventQueue().on<flux::CustomEvent>([this](flux::CustomEvent const& event) {
    if (event.type != 0x4c534850u) {
      return;
    }
    if (auto const* line = std::any_cast<std::string>(&event.payload)) {
      handleIpcLine(*line);
    }
  });
}

std::function<void(DockItem const&)> ShellController::makeActivateCallback() {
  return [this](DockItem const& item) {
    model_.activateItem(item, [this](std::string const& line) { ipc_.sendLine(line); });
    if (model_.launcherOpen()) {
      closeLauncher();
    }
  };
}

bool ShellController::connectIpc() {
  if (!ipc_.connect()) {
    return false;
  }
  ipc_.sendHello();
  ipcPollId_ = app_.registerEventPollSource(ipc_.fd(), [this] {
    ipc_.dispatchReadable([this](std::string_view line) {
      app_.eventQueue().post(flux::CustomEvent{.type = 0x4c534850u, .payload = std::string(line)});
    });
    if (!ipc_.connected()) {
      app_.quit();
    }
  });
  return ipc_.connected();
}

void ShellController::createProductionWindows() {
  auto& topBar = app_.createWindow<flux::Window>(topBarWindowConfig());
  topBar.setClearColor(flux::Colors::transparent);
  topBarWindow_ = &topBar;
  topBarHandle_ = topBar.handle();

  int const dockWidthPx = dockWidth(model_.dockItems());
  auto& dock = app_.createWindow<flux::Window>(dockWindowConfig(dockWidthPx));
  dock.setClearColor(flux::Colors::transparent);
  dockWindow_ = &dock;
  dockHandle_ = dock.handle();

  auto& launcher = app_.createWindow<flux::Window>(launcherWindowConfig());
  launcher.setClearColor(flux::Colors::transparent);
  launcherWindow_ = &launcher;
  launcherHandle_ = launcher.handle();

  remountAllViews();
  clockTimerId_ = app_.scheduleRepeatingTimer(std::chrono::seconds(30), topBarHandle_.value_or(0));
  syncLauncherWindow();
}

void ShellController::setupPreviewWindow(flux::Window& window, float width, float height) {
  launcherWindow_ = &window;
  previewWidth_ = width;
  previewHeight_ = height;
  previewHandle_ = window.handle();
  remountPreviewView();
  clockTimerId_ = app_.scheduleRepeatingTimer(std::chrono::seconds(30), previewHandle_.value_or(0));
}

void ShellController::remountTopBarView() {
  if (!topBarWindow_) return;
  topBarWindow_->setView(ShellTopBarView{model_, [this] { openLauncher(); }});
}

void ShellController::remountDockView() {
  if (!dockWindow_) return;
  dockWindow_->setView(ShellDockView{model_, [this] { openLauncher(); }, makeActivateCallback()});
}

void ShellController::remountLauncherView() {
  if (!launcherWindow_ || previewHandle_) return;
  launcherWindow_->setView(ShellLauncherView{
      model_,
      makeActivateCallback(),
      [this] { closeLauncher(); },
      {},
  });
}

void ShellController::remountPreviewView() {
  if (!launcherWindow_ || !previewHandle_) return;
  launcherWindow_->setView(ShellDesktopView{
      model_,
      [this] { openLauncher(); },
      makeActivateCallback(),
      makeActivateCallback(),
      [this] { closeLauncher(); },
      {},
      previewWidth_,
      previewHeight_,
  });
}

void ShellController::remountAllViews() {
  remountTopBarView();
  remountDockView();
  remountLauncherView();
}

void ShellController::requestRedraws() {
  if (topBarWindow_) topBarWindow_->requestRedraw();
  if (dockWindow_) dockWindow_->requestRedraw();
  if (launcherWindow_) launcherWindow_->requestRedraw();
}

void ShellController::openLauncher() {
  if (model_.launcherOpen()) return;
  model_.openLauncher();
  syncLauncherWindow();
  remountLauncherView();
  launcherWindow_->requestRedraw();
}

void ShellController::closeLauncher() {
  if (!model_.launcherOpen()) return;
  model_.closeLauncher();
  syncLauncherWindow();
  remountLauncherView();
  launcherWindow_->requestRedraw();
}

void ShellController::syncLauncherWindow() {
  if (!launcherWindow_ || previewHandle_) return;

  bool const wantOpen = model_.launcherOpen();
  if (wantOpen == lastLauncherOpen_) {
    return;
  }
  lastLauncherOpen_ = wantOpen;

  if (wantOpen) {
    launcherWindow_->resize({0.f, 0.f});
    launcherWindow_->setLayerShellKeyboardInteractive(true);
    if (ipc_.connected() && !launcherModalClaimed_) {
      ipc_.claimLauncherModal();
      launcherModalClaimed_ = true;
    }
  } else {
    if (ipc_.connected() && launcherModalClaimed_) {
      ipc_.releaseLauncherModal();
      launcherModalClaimed_ = false;
    }
    launcherWindow_->setLayerShellKeyboardInteractive(false);
    launcherWindow_->resize({1.f, 1.f});
  }
}

void ShellController::handleIpcLine(std::string_view line) {
  if (lineContains(line, "\"lambda.shell.openCommandLauncher\"")) {
    openLauncher();
    return;
  }
  if (lineContains(line, "\"lambda.windowManager.snapshot\"")) {
    model_.applySnapshot(line);
    if (dockWindow_) {
      int const width = dockWidth(model_.dockItems());
      dockWindow_->resize({static_cast<float>(width), static_cast<float>(dockHeight())});
    }
  }
}

void ShellController::handleLauncherKey(flux::InputEvent const& event) {
  if (event.kind == flux::InputEvent::Kind::KeyDown) {
    if (event.key == Escape) {
      closeLauncher();
      return;
    }
    if (event.key == Delete) {
      model_.backspaceQuery();
      remountLauncherView();
      launcherWindow_->requestRedraw();
      return;
    }
    if (event.key == DownArrow) {
      model_.moveHighlight(1);
      remountLauncherView();
      launcherWindow_->requestRedraw();
      return;
    }
    if (event.key == UpArrow) {
      model_.moveHighlight(-1);
      remountLauncherView();
      launcherWindow_->requestRedraw();
      return;
    }
    if (event.key == Return) {
      auto results = model_.launcherResults();
      if (!results.empty()) {
        int const index = std::clamp(model_.highlighted(), 0, static_cast<int>(results.size()) - 1);
        model_.activateItem(results[static_cast<std::size_t>(index)],
                            [this](std::string const& line) { ipc_.sendLine(line); });
        closeLauncher();
      }
      return;
    }
  }
  if (event.kind == flux::InputEvent::Kind::TextInput && !event.text.empty()) {
    model_.appendQueryText(event.text);
    remountLauncherView();
    launcherWindow_->requestRedraw();
  }
}

} // namespace lambda_shell
