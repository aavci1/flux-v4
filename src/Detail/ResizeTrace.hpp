#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace flux::detail {

inline bool resizeTraceEnabled() {
  static bool const enabled = [] {
    char const* value = std::getenv("FLUX_RESIZE_TRACE");
    return value && *value && std::strcmp(value, "0") != 0;
  }();
  return enabled;
}

inline void resizeTrace(char const* prefix, char const* format, ...) {
  if (!resizeTraceEnabled()) return;
  if (!prefix || !*prefix) prefix = "resize";

  auto write = [&](FILE* file, va_list args) {
    std::fprintf(file, "resize-trace: %s: ", prefix);
    std::vfprintf(file, format, args);
  };

  va_list args;
  va_start(args, format);
  va_list stderrArgs;
  va_copy(stderrArgs, args);
  write(stderr, stderrArgs);
  va_end(stderrArgs);

  char const* path = std::getenv("FLUX_RESIZE_TRACE_LOG");
  if (!path || !*path) {
    path = "/tmp/flux-resize-trace.log";
  }
  if (FILE* file = std::fopen(path, "a")) {
    write(file, args);
    std::fclose(file);
  }
  va_end(args);
}

} // namespace flux::detail
