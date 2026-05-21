#include "Compositor/Diagnostics/CpuTrace.hpp"

#include <sys/resource.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>

namespace flux::compositor::diagnostics {
namespace {

struct CpuTraceState {
  CpuTraceClock::time_point windowStart = CpuTraceClock::now();
  double cpuStartMs = 0.0;
  std::FILE* file = nullptr;
  std::uint64_t frames = 0;
  std::uint64_t loops = 0;
  std::uint64_t idleSkips = 0;
  std::uint64_t polls = 0;
  std::uint64_t pollWakeups = 0;
  std::uint64_t dispatches = 0;
  std::uint64_t surfaces = 0;
  double pollMs = 0.0;
  double dispatchMs = 0.0;
  double backgroundMs = 0.0;
  double snapshotMs = 0.0;
  double surfaceMs = 0.0;
  double closingMs = 0.0;
  double launcherMs = 0.0;
  double cursorMs = 0.0;
  double presentMs = 0.0;
  double totalMs = 0.0;
  double maxTotalMs = 0.0;
  double maxSurfaceMs = 0.0;
  double maxPresentMs = 0.0;
  std::uint64_t shmCopies = 0;
  std::size_t shmBytes = 0;
  double shmCopyMs = 0.0;
  std::uint64_t imageCreates = 0;
  std::uint64_t imageUpdates = 0;
  std::size_t imageBytes = 0;
  double imageUploadMs = 0.0;
  std::uint64_t dmabufImports = 0;
  std::uint64_t dmabufImportFailures = 0;
  double dmabufImportMs = 0.0;
  std::uint64_t dmabufFallbackCopies = 0;
  std::uint64_t dmabufFallbackFailures = 0;
  std::size_t dmabufFallbackBytes = 0;
  double dmabufFallbackMs = 0.0;
};

std::mutex& traceMutex() {
  static std::mutex mutex;
  return mutex;
}

double processCpuMilliseconds() {
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) != 0) return 0.0;
  auto const userMs = static_cast<double>(usage.ru_utime.tv_sec) * 1000.0 +
                      static_cast<double>(usage.ru_utime.tv_usec) / 1000.0;
  auto const systemMs = static_cast<double>(usage.ru_stime.tv_sec) * 1000.0 +
                        static_cast<double>(usage.ru_stime.tv_usec) / 1000.0;
  return userMs + systemMs;
}

CpuTraceState& state() {
  static CpuTraceState traceState = [] {
    CpuTraceState result;
    result.cpuStartMs = processCpuMilliseconds();
    return result;
  }();
  return traceState;
}

std::string defaultTracePath() {
  if (char const* configured = std::getenv("FLUX_COMPOSITOR_CPU_TRACE_LOG");
      configured && *configured) {
    return configured;
  }
  if (char const* stateHome = std::getenv("XDG_STATE_HOME"); stateHome && *stateHome) {
    return std::string(stateHome) + "/flux-compositor/cpu.log";
  }
  if (char const* home = std::getenv("HOME"); home && *home) {
    return std::string(home) + "/.local/state/flux-compositor/cpu.log";
  }
  return "/tmp/flux-compositor-cpu.log";
}

char const* tracePath() {
  static std::string const path = defaultTracePath();
  return path.c_str();
}

char const* fallbackTracePath() {
  char const* path = std::getenv("FLUX_COMPOSITOR_CPU_TRACE_LOG");
  return path && *path ? path : "/tmp/flux-compositor-cpu.log";
}

std::FILE* traceFile(CpuTraceState& traceState) {
  if (traceState.file) return traceState.file;
  try {
    std::filesystem::path const path(tracePath());
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
  } catch (...) {
  }
  traceState.file = std::fopen(tracePath(), "a");
  if (!traceState.file && std::strcmp(tracePath(), fallbackTracePath()) != 0) {
    traceState.file = std::fopen(fallbackTracePath(), "a");
  }
  return traceState.file;
}

void resetCounters(CpuTraceState& traceState, CpuTraceClock::time_point now, double cpuNowMs) {
  traceState.windowStart = now;
  traceState.cpuStartMs = cpuNowMs;
  traceState.frames = 0;
  traceState.loops = 0;
  traceState.idleSkips = 0;
  traceState.polls = 0;
  traceState.pollWakeups = 0;
  traceState.dispatches = 0;
  traceState.surfaces = 0;
  traceState.pollMs = 0.0;
  traceState.dispatchMs = 0.0;
  traceState.backgroundMs = 0.0;
  traceState.snapshotMs = 0.0;
  traceState.surfaceMs = 0.0;
  traceState.closingMs = 0.0;
  traceState.launcherMs = 0.0;
  traceState.cursorMs = 0.0;
  traceState.presentMs = 0.0;
  traceState.totalMs = 0.0;
  traceState.maxTotalMs = 0.0;
  traceState.maxSurfaceMs = 0.0;
  traceState.maxPresentMs = 0.0;
  traceState.shmCopies = 0;
  traceState.shmBytes = 0;
  traceState.shmCopyMs = 0.0;
  traceState.imageCreates = 0;
  traceState.imageUpdates = 0;
  traceState.imageBytes = 0;
  traceState.imageUploadMs = 0.0;
  traceState.dmabufImports = 0;
  traceState.dmabufImportFailures = 0;
  traceState.dmabufImportMs = 0.0;
  traceState.dmabufFallbackCopies = 0;
  traceState.dmabufFallbackFailures = 0;
  traceState.dmabufFallbackBytes = 0;
  traceState.dmabufFallbackMs = 0.0;
}

void maybeLog(CpuTraceState& traceState) {
  auto const now = CpuTraceClock::now();
  double const elapsedMs = cpuTraceElapsedMilliseconds(traceState.windowStart);
  if (elapsedMs < 1000.0) return;

  double const cpuNowMs = processCpuMilliseconds();
  double const cpuPercent = elapsedMs > 0.0 ? (cpuNowMs - traceState.cpuStartMs) * 100.0 / elapsedMs : 0.0;
  double const seconds = elapsedMs / 1000.0;
  double const invFrames = traceState.frames > 0 ? 1.0 / static_cast<double>(traceState.frames) : 0.0;
  double const shmMb = static_cast<double>(traceState.shmBytes) / (1024.0 * 1024.0);
  double const imageMb = static_cast<double>(traceState.imageBytes) / (1024.0 * 1024.0);
  double const fallbackMb = static_cast<double>(traceState.dmabufFallbackBytes) / (1024.0 * 1024.0);

  if (std::FILE* file = traceFile(traceState)) {
    std::fprintf(file,
                 "cpu-trace: window=%.2fs cpu=%.1f%% loops=%llu idle_skips=%llu frames=%llu "
                 "fps=%.1f polls=%llu wakeups=%llu poll_ms=%.3f dispatches=%llu dispatch_ms=%.3f "
                 "surfaces=%.2f/f "
                 "phase_avg_ms total=%.3f bg=%.3f snapshot=%.3f surface=%.3f closing=%.3f "
                 "launcher=%.3f cursor=%.3f present=%.3f max_total=%.3f max_surface=%.3f "
                 "max_present=%.3f shm copies=%llu mb=%.1f mbps=%.1f copy_ms=%.3f "
                 "image creates=%llu updates=%llu mb=%.1f mbps=%.1f upload_ms=%.3f "
                 "dmabuf imports=%llu failures=%llu import_ms=%.3f fallback_copies=%llu "
                 "fallback_failures=%llu fallback_mb=%.1f fallback_ms=%.3f\n",
                 seconds,
                 cpuPercent,
                 static_cast<unsigned long long>(traceState.loops),
                 static_cast<unsigned long long>(traceState.idleSkips),
                 static_cast<unsigned long long>(traceState.frames),
                 static_cast<double>(traceState.frames) / seconds,
                 static_cast<unsigned long long>(traceState.polls),
                 static_cast<unsigned long long>(traceState.pollWakeups),
                 traceState.pollMs,
                 static_cast<unsigned long long>(traceState.dispatches),
                 traceState.dispatchMs,
                 static_cast<double>(traceState.surfaces) * invFrames,
                 traceState.totalMs * invFrames,
                 traceState.backgroundMs * invFrames,
                 traceState.snapshotMs * invFrames,
                 traceState.surfaceMs * invFrames,
                 traceState.closingMs * invFrames,
                 traceState.launcherMs * invFrames,
                 traceState.cursorMs * invFrames,
                 traceState.presentMs * invFrames,
                 traceState.maxTotalMs,
                 traceState.maxSurfaceMs,
                 traceState.maxPresentMs,
                 static_cast<unsigned long long>(traceState.shmCopies),
                 shmMb,
                 shmMb / seconds,
                 traceState.shmCopyMs,
                 static_cast<unsigned long long>(traceState.imageCreates),
                 static_cast<unsigned long long>(traceState.imageUpdates),
                 imageMb,
                 imageMb / seconds,
                 traceState.imageUploadMs,
                 static_cast<unsigned long long>(traceState.dmabufImports),
                 static_cast<unsigned long long>(traceState.dmabufImportFailures),
                 traceState.dmabufImportMs,
                 static_cast<unsigned long long>(traceState.dmabufFallbackCopies),
                 static_cast<unsigned long long>(traceState.dmabufFallbackFailures),
                 fallbackMb,
                 traceState.dmabufFallbackMs);
    std::fflush(file);
    fsync(fileno(file));
  }

  resetCounters(traceState, now, cpuNowMs);
}

} // namespace

bool cpuTraceEnabled() noexcept {
  static bool const enabled = [] {
    char const* value = std::getenv("FLUX_COMPOSITOR_CPU_TRACE");
    return !value || !*value || std::strcmp(value, "0") != 0;
  }();
  return enabled;
}

char const* cpuTracePath() noexcept {
  return tracePath();
}

CpuTraceClock::time_point cpuTraceNow() noexcept {
  return CpuTraceClock::now();
}

double cpuTraceElapsedMilliseconds(CpuTraceClock::time_point start) noexcept {
  return std::chrono::duration<double, std::milli>(CpuTraceClock::now() - start).count();
}

void recordCpuFrame(CpuFrameTrace const& frame) {
  if (!cpuTraceEnabled()) return;
  std::scoped_lock lock(traceMutex());
  auto& traceState = state();
  ++traceState.frames;
  traceState.surfaces += frame.surfaces;
  traceState.backgroundMs += frame.backgroundMs;
  traceState.snapshotMs += frame.snapshotMs;
  traceState.surfaceMs += frame.surfaceMs;
  traceState.closingMs += frame.closingMs;
  traceState.launcherMs += frame.launcherMs;
  traceState.cursorMs += frame.cursorMs;
  traceState.presentMs += frame.presentMs;
  traceState.totalMs += frame.totalMs;
  traceState.maxTotalMs = std::max(traceState.maxTotalMs, frame.totalMs);
  traceState.maxSurfaceMs = std::max(traceState.maxSurfaceMs, frame.surfaceMs);
  traceState.maxPresentMs = std::max(traceState.maxPresentMs, frame.presentMs);
  maybeLog(traceState);
}

void recordCpuLoop() {
  if (!cpuTraceEnabled()) return;
  std::scoped_lock lock(traceMutex());
  auto& traceState = state();
  ++traceState.loops;
  if ((traceState.loops & 63ull) == 0ull) maybeLog(traceState);
}

void recordCpuIdleSkip() {
  if (!cpuTraceEnabled()) return;
  std::scoped_lock lock(traceMutex());
  ++state().idleSkips;
}

void recordCpuPoll(double milliseconds, bool woke) {
  if (!cpuTraceEnabled()) return;
  std::scoped_lock lock(traceMutex());
  auto& traceState = state();
  ++traceState.polls;
  if (woke) ++traceState.pollWakeups;
  traceState.pollMs += milliseconds;
}

void recordCpuDispatch(double milliseconds) {
  if (!cpuTraceEnabled()) return;
  std::scoped_lock lock(traceMutex());
  auto& traceState = state();
  ++traceState.dispatches;
  traceState.dispatchMs += milliseconds;
}

void recordShmCopy(std::size_t bytes, double milliseconds) {
  if (!cpuTraceEnabled()) return;
  std::scoped_lock lock(traceMutex());
  auto& traceState = state();
  ++traceState.shmCopies;
  traceState.shmBytes += bytes;
  traceState.shmCopyMs += milliseconds;
}

void recordSurfaceImageUpload(std::size_t bytes, double milliseconds, bool created) {
  if (!cpuTraceEnabled()) return;
  std::scoped_lock lock(traceMutex());
  auto& traceState = state();
  if (created) {
    ++traceState.imageCreates;
  } else {
    ++traceState.imageUpdates;
  }
  traceState.imageBytes += bytes;
  traceState.imageUploadMs += milliseconds;
}

void recordDmabufImport(double milliseconds, bool imported) {
  if (!cpuTraceEnabled()) return;
  std::scoped_lock lock(traceMutex());
  auto& traceState = state();
  if (imported) {
    ++traceState.dmabufImports;
  } else {
    ++traceState.dmabufImportFailures;
  }
  traceState.dmabufImportMs += milliseconds;
}

void recordDmabufFallbackCopy(std::size_t bytes, double milliseconds, bool success) {
  if (!cpuTraceEnabled()) return;
  std::scoped_lock lock(traceMutex());
  auto& traceState = state();
  if (success) {
    ++traceState.dmabufFallbackCopies;
    traceState.dmabufFallbackBytes += bytes;
  } else {
    ++traceState.dmabufFallbackFailures;
  }
  traceState.dmabufFallbackMs += milliseconds;
}

} // namespace flux::compositor::diagnostics
