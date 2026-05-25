#include "Shell/ShellModels.hpp"

#include <doctest/doctest.h>

TEST_CASE("Shell dock model combines pinned apps and running windows") {
  std::vector<lambda_shell::AppRegistryEntry> pinned{
      {.appId = "files", .name = "Files", .icon = "files"},
      {.appId = "terminal", .name = "Terminal", .icon = "terminal"},
  };
  std::vector<lambda_shell::ShellWindowSnapshot> windows{
      {.id = 10, .appId = "lambda-files", .title = "Files", .focused = true},
      {.id = 11, .appId = "org.example.Editor", .title = "Editor"},
      {.id = 12, .appId = "lambda-terminal", .title = "Terminal", .minimized = true},
  };

  auto dock = lambda_shell::buildDockModel(pinned, windows);
  REQUIRE(dock.size() == 3);
  CHECK(dock[0].appId == "files");
  CHECK(dock[0].running);
  CHECK(dock[0].focused);
  CHECK(dock[0].windowIds == std::vector<std::uint64_t>{10});
  CHECK(dock[1].appId == "terminal");
  CHECK(dock[1].minimized);
  CHECK(dock[2].appId == "org.example.Editor");
  CHECK_FALSE(dock[2].pinned);
}

TEST_CASE("Shell dock click behavior launches focuses or restores") {
  CHECK(lambda_shell::dockClickAction({.appId = "files"}).kind == lambda_shell::DockClickKind::LaunchApp);
  CHECK(lambda_shell::dockClickAction({.appId = "files", .running = true}).kind ==
        lambda_shell::DockClickKind::FocusApp);
  CHECK(lambda_shell::dockClickAction({.appId = "files", .running = true, .minimized = true, .windowIds = {1}}).kind ==
        lambda_shell::DockClickKind::RestoreApp);
}

TEST_CASE("Shell launcher ranking handles prefix acronym fuzzy running and recent boosts") {
  std::vector<lambda_shell::AppRegistryEntry> apps{
      {.appId = "lambda-files", .name = "Files", .keywords = {"folder"}},
      {.appId = "lambda-settings", .name = "System Settings", .keywords = {"preferences"}},
      {.appId = "lambda-terminal", .name = "Terminal"},
      {.appId = "org.example.Hidden", .name = "Hidden", .hidden = true},
  };
  std::vector<lambda_shell::ShellWindowSnapshot> windows{{.id = 1, .appId = "lambda-terminal"}};
  auto prefix = lambda_shell::rankLauncherApps(apps, windows, {}, "fi");
  REQUIRE(prefix.size() == 1);
  CHECK(prefix[0].app.appId == "lambda-files");

  auto acronym = lambda_shell::rankLauncherApps(apps, windows, {}, "ss");
  REQUIRE(acronym.size() == 1);
  CHECK(acronym[0].app.appId == "lambda-settings");

  auto running = lambda_shell::rankLauncherApps(apps, windows, {"lambda-files"}, "ter");
  REQUIRE(running.size() == 1);
  CHECK(running[0].app.appId == "lambda-terminal");
  CHECK(running[0].running);

  auto empty = lambda_shell::rankLauncherApps(apps, windows, {"lambda-files"}, "");
  REQUIRE(empty.size() == 3);
  CHECK(empty[0].app.appId == "lambda-terminal");
  CHECK(empty[1].app.appId == "lambda-files");
}

TEST_CASE("Shell notification model groups dismisses clears and honors DND") {
  lambda_shell::NotificationCenterModel notifications{2};
  auto first = notifications.add("files", "Done", "Copied");
  auto second = notifications.add("files", "Done", "Moved");
  auto third = notifications.add("terminal", "Build", "Failed");

  CHECK(first == 1);
  CHECK(second == 2);
  CHECK(third == 3);
  CHECK(notifications.history().size() == 2);
  CHECK(notifications.groupCount("files") == 1);
  CHECK(notifications.visible().size() == 2);

  CHECK(notifications.dismiss(second));
  CHECK(notifications.groupCount("files") == 0);
  notifications.setDoNotDisturb(true);
  CHECK(notifications.visible().empty());
  notifications.setDoNotDisturb(false);
  notifications.clearAll();
  CHECK(notifications.visible().empty());
}

TEST_CASE("Shell clipboard history dedupes respects limits and disabled state") {
  lambda_shell::ClipboardHistoryModel clipboard{3};
  clipboard.addText("one");
  clipboard.addText("two");
  clipboard.addText("three");
  clipboard.addText("four");
  CHECK(clipboard.entries() == std::vector<std::string>{"four", "three", "two"});

  clipboard.addText("three");
  CHECK(clipboard.entries() == std::vector<std::string>{"three", "four", "two"});

  clipboard.setEnabled(false);
  clipboard.addText("ignored");
  CHECK(clipboard.entries() == std::vector<std::string>{"three", "four", "two"});

  clipboard.clear();
  CHECK(clipboard.entries().empty());
}
