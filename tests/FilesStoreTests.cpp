#include "FilesStore.hpp"

#include <doctest/doctest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

struct ScopedEnv {
  explicit ScopedEnv(char const* name)
      : name(name) {
    if (char const* value = std::getenv(name); value) {
      hadOriginal = true;
      original = value;
    }
  }

  ~ScopedEnv() {
    if (!hadOriginal) {
      unsetenv(name);
    } else {
      setenv(name, original.c_str(), 1);
    }
  }

  char const* name;
  bool hadOriginal = false;
  std::string original;
};

std::filesystem::path tempRoot(char const* name) {
  auto path = std::filesystem::temp_directory_path() /
              (std::string(name) + "-" + std::to_string(static_cast<unsigned long long>(getpid())));
  std::filesystem::remove_all(path);
  std::filesystem::create_directories(path);
  return path;
}

} // namespace

TEST_CASE("FilesStore parses XDG user directories") {
  std::filesystem::path const home = "/home/tester";
  auto dirs = lambda_files::parseXdgUserDirs(R"(
XDG_DESKTOP_DIR="$HOME/Desktop"
XDG_DOWNLOAD_DIR="$HOME/Downloads"
XDG_DOCUMENTS_DIR="/data/Documents"
XDG_TEMPLATES_DIR=~/Templates
BROKEN=value
)",
                                             home);

  CHECK(dirs.at("desktop") == home / "Desktop");
  CHECK(dirs.at("download") == home / "Downloads");
  CHECK(dirs.at("documents") == "/data/Documents");
  CHECK(dirs.at("templates") == home / "Templates");
  CHECK_FALSE(dirs.contains("broken"));
}

TEST_CASE("FilesStore home directory falls back when HOME is unusable") {
  ScopedEnv home("HOME");
  setenv("HOME", "/path/that/does/not/exist/lambda", 1);
  CHECK(lambda_files::homeDirectory() == std::filesystem::current_path());
}

TEST_CASE("FilesStore breadcrumbs handle home root and outside home") {
  ScopedEnv homeEnv("HOME");
  auto root = tempRoot("lambda-files-breadcrumb-test");
  auto home = root / "home";
  auto nested = home / "Projects" / "Flux";
  auto outside = root / "outside" / "Folder";
  std::filesystem::create_directories(nested);
  std::filesystem::create_directories(outside);
  setenv("HOME", home.c_str(), 1);

  auto homeCrumbs = lambda_files::breadcrumbCrumbs(home);
  REQUIRE(homeCrumbs.size() == 1);
  CHECK(homeCrumbs[0] == lambda_files::BreadcrumbCrumb{"Home", home});

  auto nestedCrumbs = lambda_files::breadcrumbCrumbs(nested);
  REQUIRE(nestedCrumbs.size() == 3);
  CHECK(nestedCrumbs[0] == lambda_files::BreadcrumbCrumb{"Home", home});
  CHECK(nestedCrumbs[1].label == "Projects");
  CHECK(nestedCrumbs[2].path == nested);

  auto rootCrumbs = lambda_files::breadcrumbCrumbs("/");
  REQUIRE(rootCrumbs.size() == 1);
  CHECK(rootCrumbs[0].label == "/");

  auto outsideCrumbs = lambda_files::breadcrumbCrumbs(outside);
  REQUIRE(outsideCrumbs.size() >= 3);
  CHECK(outsideCrumbs[0].label == "/");
  CHECK(outsideCrumbs.back().label == "Folder");
  CHECK(outsideCrumbs.back().path == outside);

  std::filesystem::remove_all(root);
}

TEST_CASE("FilesStore sorts entries by name kind size and modified time") {
  using lambda_files::FileEntry;
  using lambda_files::FileSortKey;
  using lambda_files::FileVisualKind;
  auto now = std::filesystem::file_time_type::clock::now();
  std::vector<FileEntry> entries{
      {.name = "zeta.txt", .path = "/tmp/zeta.txt", .isDirectory = false, .size = 20, .modifiedAt = now,
       .visualKind = FileVisualKind::Generic},
      {.name = "Alpha", .path = "/tmp/Alpha", .isDirectory = true, .size = 0, .modifiedAt = now,
       .visualKind = FileVisualKind::Folder},
      {.name = "image.png", .path = "/tmp/image.png", .isDirectory = false, .size = 5,
       .modifiedAt = now + std::chrono::seconds(1), .visualKind = FileVisualKind::Image},
      {.name = "book.pdf", .path = "/tmp/book.pdf", .isDirectory = false, .size = 100,
       .modifiedAt = now - std::chrono::seconds(1), .visualKind = FileVisualKind::Pdf},
  };

  auto byName = lambda_files::sortedEntries(entries);
  CHECK(byName[0].name == "Alpha");
  CHECK(byName[1].name == "book.pdf");

  auto bySizeDescending = lambda_files::sortedEntries(entries, FileSortKey::Size, false, false);
  CHECK(bySizeDescending[0].name == "book.pdf");
  CHECK(bySizeDescending.back().name == "Alpha");

  auto byKind = lambda_files::sortedEntries(entries, FileSortKey::Kind, true, false);
  CHECK(byKind[0].visualKind == FileVisualKind::Folder);
  CHECK(byKind[1].visualKind == FileVisualKind::Generic);

  auto byModified = lambda_files::sortedEntries(entries, FileSortKey::ModifiedTime, true, false);
  CHECK(byModified[0].name == "book.pdf");
  CHECK(byModified.back().name == "image.png");
}

TEST_CASE("FilesStore directory listing records modified time and keeps folder-first name order") {
  auto root = tempRoot("lambda-files-listing-test");
  std::filesystem::create_directories(root / "Beta");
  std::filesystem::create_directories(root / "alpha");
  {
    std::ofstream(root / "zeta.txt") << "z";
    std::ofstream(root / ".hidden") << "h";
  }

  auto visible = lambda_files::listDirectory(root, false);
  REQUIRE(visible.error.empty());
  REQUIRE(visible.entries.size() == 3);
  CHECK(visible.entries[0].name == "alpha");
  CHECK(visible.entries[1].name == "Beta");
  CHECK(visible.entries[2].name == "zeta.txt");
  CHECK(visible.entries[2].modifiedAt != std::filesystem::file_time_type{});

  auto hidden = lambda_files::listDirectory(root, true);
  CHECK(hidden.entries.size() == 4);

  std::filesystem::remove_all(root);
}
