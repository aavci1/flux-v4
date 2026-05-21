#pragma once

#include <chrono>
#include <cstddef>

namespace flux::compositor::diagnostics {

using CpuTraceClock = std::chrono::steady_clock;

struct CpuFrameTrace {
  std::size_t surfaces = 0;
  double backgroundMs = 0.0;
  double snapshotMs = 0.0;
  double surfaceMs = 0.0;
  double closingMs = 0.0;
  double launcherMs = 0.0;
  double cursorMs = 0.0;
  double presentMs = 0.0;
  double totalMs = 0.0;
};

bool cpuTraceEnabled() noexcept;
char const* cpuTracePath() noexcept;
CpuTraceClock::time_point cpuTraceNow() noexcept;
double cpuTraceElapsedMilliseconds(CpuTraceClock::time_point start) noexcept;

void recordCpuFrame(CpuFrameTrace const& frame);
void recordCpuLoop();
void recordCpuIdleSkip();
void recordCpuPoll(double milliseconds, bool woke);
void recordCpuDispatch(double milliseconds);
void recordShmCopy(std::size_t bytes, double milliseconds);
void recordSurfaceImageUpload(std::size_t bytes, double milliseconds, bool created);
void recordDmabufImport(double milliseconds, bool imported);
void recordDmabufFallbackCopy(std::size_t bytes, double milliseconds, bool success);

} // namespace flux::compositor::diagnostics
