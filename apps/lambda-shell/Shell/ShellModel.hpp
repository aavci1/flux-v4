#pragma once

#include "Shell/UI/LambdaShellTypes.hpp"

#include <Lambda/Reactive/Signal.hpp>

#include <ctime>
#include <functional>
#include <string>
#include <vector>

namespace lambda_shell {

struct AppRegistryEntry;
struct ShellConfig;

class ShellModel {
public:
  struct SnapshotChanges {
    bool dockItems = false;
    bool activeTitle = false;
    bool systemStatus = false;

    [[nodiscard]] bool any() const { return dockItems || activeTitle || systemStatus; }
  };

  std::vector<DockItem> const& dockItems() const { return dockItems_.peek(); }
  bool launcherOpen() const { return launcherOpen_.peek(); }
  bool launcherUiVisible() const { return launcherUiVisible_.peek(); }
  float launcherWidth() const { return launcherWidth_.peek(); }
  float launcherHeight() const { return launcherHeight_.peek(); }
  float topBarWidth() const { return topBarWidth_.peek(); }
  std::string const& query() const { return query_.peek(); }
  int queryCursor() const { return queryCursor_.peek(); }
  int highlighted() const { return highlighted_.peek(); }
  std::string const& activeTitle() const { return activeTitle_.peek(); }
  std::string const& timeText() const { return timeText_.peek(); }
  lambda_shell::SystemStatus const& systemStatus() const { return systemStatus_.peek(); }
  std::vector<DockItem> const& launcherResults() const { return launcherResults_.peek(); }

  lambda::Signal<std::vector<DockItem>>& dockItemsSignal() { return dockItems_; }
  lambda::Signal<bool>& launcherOpenSignal() { return launcherOpen_; }
  lambda::Signal<bool>& launcherUiVisibleSignal() { return launcherUiVisible_; }
  lambda::Signal<float>& launcherWidthSignal() { return launcherWidth_; }
  lambda::Signal<float>& launcherHeightSignal() { return launcherHeight_; }
  lambda::Signal<float>& topBarWidthSignal() { return topBarWidth_; }
  lambda::Signal<std::string>& querySignal() { return query_; }
  lambda::Signal<int>& queryCursorSignal() { return queryCursor_; }
  lambda::Signal<int>& highlightedSignal() { return highlighted_; }
  lambda::Signal<std::string>& activeTitleSignal() { return activeTitle_; }
  lambda::Signal<std::string>& timeTextSignal() { return timeText_; }
  lambda::Signal<SystemStatus>& systemStatusSignal() { return systemStatus_; }
  lambda::Signal<std::vector<DockItem>>& launcherResultsSignal() { return launcherResults_; }

  static std::string formatTimeText();

  void resetDockItems();
  void setDockItems(std::vector<AppRegistryEntry> const& apps, ShellConfig const& config);
  [[nodiscard]] bool setDockDpiScale(float scale);
  void setPreviewFocus(std::string_view appId);
  [[nodiscard]] SnapshotChanges applySnapshot(std::string_view json);
  [[nodiscard]] bool refreshTimeText();
  void openLauncher();
  void closeLauncher();
  void setLauncherUiVisible(bool visible);
  void setLauncherSize(float width, float height);
  bool setTopBarWidth(float width);
  void setQuery(std::string query);
  void setHighlighted(int index);
  void moveHighlight(int delta);
  void appendQueryText(std::string_view text);
  void backspaceQuery();
  void deleteQueryForward();
  void moveQueryCursor(int delta);
  void moveQueryCursorToStart();
  void moveQueryCursorToEnd();

  void activateItem(DockItem const& item,
                    std::function<void(std::string const& line)> sendIpc,
                    std::uint64_t requestId = 0);

private:
  void refreshLauncherResults();
  static bool dockItemsVisualStateEqual(std::vector<DockItem> const& a,
                                        std::vector<DockItem> const& b);

  lambda::Signal<std::vector<DockItem>> dockItems_;
  lambda::Signal<bool> launcherOpen_{false};
  lambda::Signal<bool> launcherUiVisible_{false};
  lambda::Signal<float> launcherWidth_{1.f};
  lambda::Signal<float> launcherHeight_{1.f};
  lambda::Signal<float> topBarWidth_{1.f};
  lambda::Signal<std::string> query_;
  lambda::Signal<int> queryCursor_{0};
  lambda::Signal<int> highlighted_{0};
  lambda::Signal<std::string> activeTitle_;
  lambda::Signal<std::string> timeText_{formatTimeText()};
  lambda::Signal<SystemStatus> systemStatus_;
  lambda::Signal<std::vector<DockItem>> launcherResults_;
  bool showRunningUnpinned_ = true;
  std::string iconTheme_;
  int iconSize_ = 48;
  float dockDpiScale_ = 1.f;
};

} // namespace lambda_shell
