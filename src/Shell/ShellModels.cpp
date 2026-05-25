#include "Shell/ShellModels.hpp"

#include <algorithm>
#include <cctype>
#include <set>

namespace lambda_shell {
namespace {

std::string lowerAscii(std::string_view value) {
  std::string output(value);
  std::transform(output.begin(), output.end(), output.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return output;
}

bool containsCaseInsensitive(std::string_view haystack, std::string_view needle) {
  return lowerAscii(haystack).find(lowerAscii(needle)) != std::string::npos;
}

std::string acronym(std::string_view text) {
  std::string out;
  bool atWord = true;
  for (char ch : text) {
    if (std::isalnum(static_cast<unsigned char>(ch))) {
      if (atWord) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
      atWord = false;
    } else {
      atWord = true;
    }
  }
  return out;
}

bool fuzzyMatch(std::string_view haystack, std::string_view needle) {
  std::string h = lowerAscii(haystack);
  std::string n = lowerAscii(needle);
  std::size_t cursor = 0;
  for (char ch : n) {
    cursor = h.find(ch, cursor);
    if (cursor == std::string::npos) return false;
    ++cursor;
  }
  return true;
}

bool appRunning(AppRegistryEntry const& app, std::vector<ShellWindowSnapshot> const& windows) {
  return std::any_of(windows.begin(), windows.end(), [&](auto const& window) {
    return shellAppIdMatches(app.appId, window.appId);
  });
}

int recentBoost(AppRegistryEntry const& app, std::vector<std::string> const& recentAppIds) {
  for (std::size_t i = 0; i < recentAppIds.size(); ++i) {
    if (shellAppIdMatches(app.appId, recentAppIds[i])) return static_cast<int>(50u - std::min<std::size_t>(i, 50u));
  }
  return 0;
}

int queryScore(AppRegistryEntry const& app, std::string_view query) {
  if (query.empty()) return 10;
  std::string q = lowerAscii(query);
  std::string name = lowerAscii(app.name);
  std::string id = lowerAscii(app.appId);
  if (name.starts_with(q) || id.starts_with(q)) return 1000;
  if (acronym(app.name).starts_with(q)) return 850;
  for (auto const& keyword : app.keywords) {
    if (lowerAscii(keyword).starts_with(q)) return 760;
  }
  if (containsCaseInsensitive(app.name, q) || containsCaseInsensitive(app.appId, q)) return 700;
  if (fuzzyMatch(app.name, q) || fuzzyMatch(app.appId, q)) return 600;
  return 0;
}

} // namespace

NotificationCenterModel::NotificationCenterModel(std::size_t historyLimit)
    : historyLimit_(std::max<std::size_t>(1u, historyLimit)) {}

std::uint64_t NotificationCenterModel::add(std::string appId, std::string title, std::string body) {
  std::uint64_t id = nextId_++;
  notifications_.insert(notifications_.begin(),
                        Notification{.id = id,
                                     .appId = std::move(appId),
                                     .title = std::move(title),
                                     .body = std::move(body)});
  if (notifications_.size() > historyLimit_) notifications_.resize(historyLimit_);
  return id;
}

bool NotificationCenterModel::dismiss(std::uint64_t id) {
  for (auto& notification : notifications_) {
    if (notification.id == id) {
      notification.dismissed = true;
      return true;
    }
  }
  return false;
}

void NotificationCenterModel::clearAll() {
  for (auto& notification : notifications_) {
    notification.dismissed = true;
  }
}

std::vector<Notification> NotificationCenterModel::visible() const {
  if (doNotDisturb_) return {};
  std::vector<Notification> output;
  for (auto const& notification : notifications_) {
    if (!notification.dismissed) output.push_back(notification);
  }
  return output;
}

int NotificationCenterModel::groupCount(std::string_view appId) const {
  return static_cast<int>(std::count_if(notifications_.begin(), notifications_.end(), [&](auto const& notification) {
    return !notification.dismissed && notification.appId == appId;
  }));
}

ClipboardHistoryModel::ClipboardHistoryModel(std::size_t limit)
    : limit_(std::max<std::size_t>(1u, limit)) {}

void ClipboardHistoryModel::addText(std::string text) {
  if (!enabled_ || text.empty()) return;
  entries_.erase(std::remove(entries_.begin(), entries_.end(), text), entries_.end());
  entries_.insert(entries_.begin(), std::move(text));
  if (entries_.size() > limit_) entries_.resize(limit_);
}

void ClipboardHistoryModel::clear() {
  entries_.clear();
}

std::vector<DockModelEntry> buildDockModel(std::vector<AppRegistryEntry> const& pinnedApps,
                                           std::vector<ShellWindowSnapshot> const& windows) {
  std::vector<DockModelEntry> entries;
  for (auto const& app : pinnedApps) {
    DockModelEntry entry;
    entry.appId = app.appId;
    entry.name = app.name;
    entry.icon = app.icon;
    entry.pinned = true;
    for (auto const& window : windows) {
      if (!shellAppIdMatches(app.appId, window.appId)) continue;
      entry.running = true;
      entry.focused = entry.focused || window.focused;
      entry.minimized = entry.minimized || window.minimized;
      entry.windowIds.push_back(window.id);
    }
    entries.push_back(std::move(entry));
  }

  for (auto const& window : windows) {
    bool represented = std::any_of(entries.begin(), entries.end(), [&](auto const& entry) {
      return shellAppIdMatches(entry.appId, window.appId);
    });
    if (represented) continue;
    entries.push_back(DockModelEntry{
        .appId = window.appId,
        .name = window.title.empty() ? window.appId : window.title,
        .icon = window.appId,
        .pinned = false,
        .running = true,
        .focused = window.focused,
        .minimized = window.minimized,
        .windowIds = {window.id},
    });
  }
  return entries;
}

DockClickAction dockClickAction(DockModelEntry const& entry) {
  if (entry.appId.empty()) return {};
  if (!entry.running) return {.kind = DockClickKind::LaunchApp, .appId = entry.appId};
  if (entry.minimized && !entry.windowIds.empty()) return {.kind = DockClickKind::RestoreApp, .appId = entry.appId};
  return {.kind = DockClickKind::FocusApp, .appId = entry.appId};
}

std::vector<LauncherRankedResult> rankLauncherApps(std::vector<AppRegistryEntry> const& apps,
                                                   std::vector<ShellWindowSnapshot> const& windows,
                                                   std::vector<std::string> const& recentAppIds,
                                                   std::string_view query,
                                                   std::size_t limit) {
  std::vector<LauncherRankedResult> ranked;
  for (auto const& app : apps) {
    if (app.hidden || app.noDisplay) continue;
    int score = queryScore(app, query);
    if (score == 0) continue;
    bool running = appRunning(app, windows);
    if (running) score += 100;
    score += recentBoost(app, recentAppIds);
    ranked.push_back({.app = app, .score = score, .running = running});
  }
  std::stable_sort(ranked.begin(), ranked.end(), [](auto const& a, auto const& b) {
    if (a.score != b.score) return a.score > b.score;
    return lowerAscii(a.app.name) < lowerAscii(b.app.name);
  });
  if (ranked.size() > limit) ranked.resize(limit);
  return ranked;
}

} // namespace lambda_shell
