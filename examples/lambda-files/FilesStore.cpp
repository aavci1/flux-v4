#include "FilesStore.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <sstream>
#include <system_error>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace lambda_files {
namespace {

std::optional<std::filesystem::path> pathIfExists(std::filesystem::path path) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || ec) {
    return std::nullopt;
  }
  return std::filesystem::weakly_canonical(path, ec);
}

std::filesystem::path pathFromEnv(char const* name) {
  if (char const* value = std::getenv(name); value && *value) {
    return std::filesystem::path(value);
  }
  return {};
}

bool ciLess(std::string const& a, std::string const& b) {
  return std::lexicographical_compare(
      a.begin(), a.end(), b.begin(), b.end(), [](unsigned char lhs, unsigned char rhs) {
        return std::tolower(lhs) < std::tolower(rhs);
      });
}

std::string shellQuote(std::string_view text) {
  std::string quoted = "'";
  for (char ch : text) {
    if (ch == '\'') {
      quoted += "'\\''";
    } else {
      quoted.push_back(ch);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

std::string trim(std::string_view value) {
  std::size_t begin = 0;
  while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
  std::size_t end = value.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1u]))) --end;
  return std::string(value.substr(begin, end - begin));
}

std::string unquoteXdgValue(std::string_view value) {
  std::string stripped = trim(value);
  value = stripped;
  if (value.size() >= 2u && value.front() == '"' && value.back() == '"') {
    value.remove_prefix(1);
    value.remove_suffix(1);
  }
  std::string output;
  output.reserve(value.size());
  bool escaped = false;
  for (char ch : value) {
    if (escaped) {
      output.push_back(ch);
      escaped = false;
      continue;
    }
    if (ch == '\\') {
      escaped = true;
      continue;
    }
    output.push_back(ch);
  }
  if (escaped) output.push_back('\\');
  return output;
}

bool isInsideOrEqual(std::filesystem::path const& path, std::filesystem::path const& root) {
  auto pathIt = path.begin();
  auto rootIt = root.begin();
  for (; rootIt != root.end(); ++rootIt, ++pathIt) {
    if (pathIt == path.end() || *pathIt != *rootIt) return false;
  }
  return true;
}

int runShellCommand(std::string const& command) {
  if (command.empty()) {
    return -1;
  }
  return std::system(command.c_str());
}

std::size_t utf8ScalarByteLength(unsigned char lead) {
  if (lead < 0x80) {
    return 1;
  }
  if ((lead & 0xE0) == 0xC0) {
    return 2;
  }
  if ((lead & 0xF0) == 0xE0) {
    return 3;
  }
  if ((lead & 0xF8) == 0xF0) {
    return 4;
  }
  return 0;
}

bool validUtf8Scalar(std::string_view text, std::size_t index, std::size_t length) {
  if (length == 0 || index + length > text.size()) {
    return false;
  }
  unsigned char const lead = static_cast<unsigned char>(text[index]);
  std::uint32_t codepoint = 0;
  if (length == 1) {
    return lead < 0x80;
  }
  for (std::size_t offset = 1; offset < length; ++offset) {
    unsigned char const ch = static_cast<unsigned char>(text[index + offset]);
    if ((ch & 0xC0) != 0x80) {
      return false;
    }
  }
  if (length == 2) {
    codepoint = (static_cast<std::uint32_t>(lead & 0x1F) << 6) |
                (static_cast<unsigned char>(text[index + 1]) & 0x3F);
    return codepoint >= 0x80;
  }
  if (length == 3) {
    codepoint = (static_cast<std::uint32_t>(lead & 0x0F) << 12) |
                (static_cast<std::uint32_t>(static_cast<unsigned char>(text[index + 1]) & 0x3F) << 6) |
                (static_cast<unsigned char>(text[index + 2]) & 0x3F);
    return codepoint >= 0x800 && (codepoint < 0xD800 || codepoint > 0xDFFF);
  }
  codepoint = (static_cast<std::uint32_t>(lead & 0x07) << 18) |
              (static_cast<std::uint32_t>(static_cast<unsigned char>(text[index + 1]) & 0x3F) << 12) |
              (static_cast<std::uint32_t>(static_cast<unsigned char>(text[index + 2]) & 0x3F) << 6) |
              (static_cast<unsigned char>(text[index + 3]) & 0x3F);
  return codepoint >= 0x10000 && codepoint <= 0x10FFFF;
}

std::string sanitizedUtf8Prefix(std::string_view text, std::size_t maxScalars, bool& truncated) {
  std::string output;
  output.reserve(std::min(text.size(), maxScalars * 4));
  std::size_t index = 0;
  std::size_t scalars = 0;
  while (index < text.size()) {
    if (scalars >= maxScalars) {
      truncated = true;
      break;
    }
    unsigned char const lead = static_cast<unsigned char>(text[index]);
    std::size_t const length = utf8ScalarByteLength(lead);
    if (validUtf8Scalar(text, index, length)) {
      output.append(text.substr(index, length));
      index += length;
    } else {
      output += "\xEF\xBF\xBD";
      ++index;
    }
    ++scalars;
  }
  return output;
}

FileVisualKind visualKindForPath(std::filesystem::path const& path, bool isDirectory) {
  if (isDirectory) {
    return FileVisualKind::Folder;
  }
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (ext == ".pdf") {
    return FileVisualKind::Pdf;
  }
  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".webp" ||
      ext == ".svg" || ext == ".heic") {
    return FileVisualKind::Image;
  }
  if (ext == ".key") {
    return FileVisualKind::Presentation;
  }
  if (ext == ".sketch") {
    return FileVisualKind::Sketch;
  }
  return FileVisualKind::Generic;
}

} // namespace

std::filesystem::path homeDirectory() {
  if (auto const home = pathIfExists(pathFromEnv("HOME"))) {
    return *home;
  }
  return std::filesystem::current_path();
}

std::map<std::string, std::filesystem::path> parseXdgUserDirs(std::string_view configText,
                                                              std::filesystem::path const& home) {
  std::map<std::string, std::filesystem::path> dirs;
  std::istringstream input{std::string(configText)};
  std::string line;
  while (std::getline(input, line)) {
    std::string_view view(line);
    if (auto hash = view.find('#'); hash != std::string_view::npos) view = view.substr(0, hash);
    std::string stripped = trim(view);
    view = stripped;
    if (view.empty()) continue;
    auto equals = view.find('=');
    if (equals == std::string_view::npos) continue;
    std::string key = trim(view.substr(0, equals));
    constexpr std::string_view prefix = "XDG_";
    constexpr std::string_view suffix = "_DIR";
    if (!key.starts_with(prefix) || !key.ends_with(suffix)) continue;
    key = key.substr(prefix.size(), key.size() - prefix.size() - suffix.size());
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
      return static_cast<char>(std::tolower(ch));
    });
    std::string value = unquoteXdgValue(view.substr(equals + 1u));
    if (value.empty()) continue;
    std::filesystem::path path(value);
    std::string pathText = path.string();
    if (pathText == "$HOME") {
      path = home;
    } else if (pathText.starts_with("$HOME/")) {
      path = home / pathText.substr(6u);
    } else if (pathText.starts_with("~/")) {
      path = home / pathText.substr(2u);
    }
    dirs[key] = path.lexically_normal();
  }
  return dirs;
}

std::vector<SidebarPlace> const& sidebarPlaces() {
  static std::vector<SidebarPlace> const places = [] {
    std::filesystem::path const home = homeDirectory();
    std::vector<SidebarPlace> built;
    built.push_back({.id = "home", .label = "Home", .icon = flux::IconName::Home, .path = home});

    auto add = [&built](char const* id, char const* label, flux::IconName icon, std::filesystem::path path) {
      for (auto const& existing : built) {
        if (existing.id == id) {
          return;
        }
      }
      if (auto const resolved = pathIfExists(std::move(path))) {
        built.push_back({.id = id, .label = label, .icon = icon, .path = *resolved});
      }
    };

    add("desktop", "Desktop", flux::IconName::DesktopWindows, home / "Desktop");
    if (std::filesystem::path const docs = pathFromEnv("XDG_DOCUMENTS_DIR"); !docs.empty()) {
      add("documents", "Documents", flux::IconName::Description, docs);
    } else {
      add("documents", "Documents", flux::IconName::Description, home / "Documents");
    }
    if (std::filesystem::path const dl = pathFromEnv("XDG_DOWNLOAD_DIR"); !dl.empty()) {
      add("downloads", "Downloads", flux::IconName::Download, dl);
    } else {
      add("downloads", "Downloads", flux::IconName::Download, home / "Downloads");
    }
    return built;
  }();
  return places;
}

std::string gridDisplayName(std::string name) {
  constexpr std::size_t kMaxChars = 20;
  if (kMaxChars <= 3) {
    return "...";
  }
  bool truncated = false;
  std::string display = sanitizedUtf8Prefix(name, kMaxChars, truncated);
  if (!truncated) {
    return display;
  }
  truncated = false;
  display = sanitizedUtf8Prefix(name, kMaxChars - 3, truncated);
  display += "...";
  return display;
}

std::string normalizeDirectoryPath(std::filesystem::path path) {
  std::error_code ec;
  std::filesystem::path const canonical = std::filesystem::weakly_canonical(path, ec);
  if (!ec && !canonical.empty()) {
    path = canonical;
  }
  return path.string();
}

ListDirectoryResult listDirectory(std::filesystem::path const& directory, bool includeHidden) {
  ListDirectoryResult result;
  std::error_code ec;
  if (!std::filesystem::exists(directory, ec) || ec) {
    result.error = "Folder does not exist.";
    return result;
  }
  if (!std::filesystem::is_directory(directory, ec) || ec) {
    result.error = "Not a folder.";
    return result;
  }

  for (auto const& entry : std::filesystem::directory_iterator(directory, ec)) {
    if (ec) {
      result.error = ec.message();
      result.entries.clear();
      return result;
    }
    std::string const name = entry.path().filename().string();
    if (name == "." || name == "..") {
      continue;
    }
    if (!includeHidden && !name.empty() && name.front() == '.') {
      continue;
    }
    std::error_code typeEc;
    bool const isDir = entry.is_directory(typeEc);
    if (typeEc) {
      continue;
    }
    std::uintmax_t size = 0;
    if (!isDir) {
      size = entry.file_size(typeEc);
      if (typeEc) {
        size = 0;
      }
    }
    std::filesystem::file_time_type modifiedAt{};
    modifiedAt = entry.last_write_time(typeEc);
    if (typeEc) {
      modifiedAt = {};
    }
    result.entries.push_back(FileEntry{
        .name = name,
        .path = entry.path(),
        .isDirectory = isDir,
        .size = size,
        .modifiedAt = modifiedAt,
        .visualKind = visualKindForPath(entry.path(), isDir),
    });
  }

  result.entries = sortedEntries(std::move(result.entries));
  return result;
}

std::vector<FileEntry> sortedEntries(std::vector<FileEntry> entries,
                                     FileSortKey key,
                                     bool ascending,
                                     bool directoriesFirst) {
  auto compareField = [&](FileEntry const& a, FileEntry const& b) {
    switch (key) {
    case FileSortKey::Name:
      if (ciLess(a.name, b.name)) return true;
      if (ciLess(b.name, a.name)) return false;
      return a.name < b.name;
    case FileSortKey::Kind:
      if (a.visualKind != b.visualKind) {
        return static_cast<int>(a.visualKind) < static_cast<int>(b.visualKind);
      }
      if (ciLess(a.name, b.name)) return true;
      if (ciLess(b.name, a.name)) return false;
      return a.name < b.name;
    case FileSortKey::Size:
      if (a.size != b.size) return a.size < b.size;
      if (ciLess(a.name, b.name)) return true;
      if (ciLess(b.name, a.name)) return false;
      return a.name < b.name;
    case FileSortKey::ModifiedTime:
      if (a.modifiedAt != b.modifiedAt) return a.modifiedAt < b.modifiedAt;
      if (ciLess(a.name, b.name)) return true;
      if (ciLess(b.name, a.name)) return false;
      return a.name < b.name;
    }
    return false;
  };

  std::stable_sort(entries.begin(), entries.end(), [&](FileEntry const& a, FileEntry const& b) {
    if (directoriesFirst && a.isDirectory != b.isDirectory) return a.isDirectory;
    bool const less = compareField(a, b);
    bool const greater = compareField(b, a);
    if (!less && !greater) return false;
    return ascending ? less : greater;
  });
  return entries;
}

std::vector<BreadcrumbCrumb> breadcrumbCrumbs(std::filesystem::path const& path) {
  std::vector<BreadcrumbCrumb> crumbs;
  std::error_code ec;
  std::filesystem::path current = std::filesystem::weakly_canonical(path, ec);
  if (ec || current.empty()) {
    current = path;
  }

  std::filesystem::path const home = homeDirectory();
  if (isInsideOrEqual(current, home)) {
    crumbs.push_back({"Home", home});
    if (current == home) {
      return crumbs;
    }
    std::filesystem::path accumulated = home;
    std::filesystem::path relative = std::filesystem::relative(current, home, ec);
    if (ec) relative = current.lexically_relative(home);
    for (std::filesystem::path const& part : relative) {
      if (part.empty() || part == ".") continue;
      accumulated /= part;
      crumbs.push_back({part.string(), accumulated});
    }
    return crumbs;
  }

  std::filesystem::path accumulated = current.root_path();
  crumbs.push_back({accumulated.empty() ? "/" : accumulated.string(), accumulated.empty() ? "/" : accumulated});
  if (current == accumulated) return crumbs;
  for (std::filesystem::path const& part : current.relative_path()) {
    if (part.empty() || part == ".") {
      continue;
    }
    accumulated /= part;
    crumbs.push_back({part.string(), accumulated});
  }
  return crumbs;
}

std::optional<std::filesystem::path> parentDirectory(std::filesystem::path const& path) {
  std::error_code ec;
  std::filesystem::path const canonical = std::filesystem::weakly_canonical(path, ec);
  std::filesystem::path const current = ec ? path : canonical;
  if (!current.has_parent_path()) {
    return std::nullopt;
  }
  std::filesystem::path parent = current.parent_path();
  if (parent == current) {
    return std::nullopt;
  }
  if (!std::filesystem::exists(parent, ec) || ec || !std::filesystem::is_directory(parent, ec) || ec) {
    return std::nullopt;
  }
  return parent;
}

FileVisualKind visualKindForEntry(std::filesystem::path const& path, bool isDirectory) {
  return visualKindForPath(path, isDirectory);
}

std::string formatEntrySubtitle(FileEntry const& entry) {
  if (entry.isDirectory) {
    return "Folder";
  }
  if (entry.size < 1024) {
    return std::to_string(entry.size) + " B";
  }
  if (entry.size < 1024 * 1024) {
    return std::to_string((entry.size + 1023) / 1024) + " KB";
  }
  return std::to_string((entry.size + 1024 * 1024 - 1) / (1024 * 1024)) + " MB";
}

std::string formatSidebarFooter(std::filesystem::path const& path) {
  return normalizeDirectoryPath(path);
}

bool openEntry(FileEntry const& entry, std::string& error) {
  if (entry.isDirectory) {
    error = "Use navigation for folders.";
    return false;
  }
#if defined(__APPLE__)
  int const code = runShellCommand("open " + shellQuote(entry.path.string()));
#else
  int const code = runShellCommand("xdg-open " + shellQuote(entry.path.string()));
#endif
  if (code != 0) {
    error = "Could not open this item with the system handler.";
    return false;
  }
  return true;
}

bool revealEntryInSystem(FileEntry const& entry, std::string& error) {
#if defined(__APPLE__)
  int const code = runShellCommand("open -R " + shellQuote(entry.path.string()));
#else
  int const code = runShellCommand("xdg-open " + shellQuote(entry.path.parent_path().string()));
#endif
  if (code != 0) {
    error = "Could not reveal this item in the system file manager.";
    return false;
  }
  return true;
}

NavigationHistory navigateTo(NavigationHistory history, std::filesystem::path path) {
  std::string const next = normalizeDirectoryPath(std::move(path));
  if (history.current == next) {
    history.forward.clear();
    return history;
  }
  if (!history.current.empty()) {
    history.back.push_back(history.current);
  }
  history.current = next;
  history.forward.clear();
  return history;
}

NavigationHistory goBack(NavigationHistory history) {
  if (history.back.empty()) {
    return history;
  }
  if (!history.current.empty()) {
    history.forward.push_back(history.current);
  }
  history.current = history.back.back();
  history.back.pop_back();
  return history;
}

NavigationHistory goForward(NavigationHistory history) {
  if (history.forward.empty()) {
    return history;
  }
  if (!history.current.empty()) {
    history.back.push_back(history.current);
  }
  history.current = history.forward.back();
  history.forward.pop_back();
  return history;
}

NavigationHistory goUp(NavigationHistory history) {
  if (auto const parent = parentDirectory(std::filesystem::path(history.current))) {
    return navigateTo(std::move(history), *parent);
  }
  return history;
}

} // namespace lambda_files
