#pragma once

#include <cstdarg>

namespace flux::compositor::diagnostics {

void initializeCrashLog();
void installCrashHandlers();
bool crashLogEnabled() noexcept;
char const* crashLogPath() noexcept;
void crashLog(char const* format, ...);
void crashLogV(char const* format, va_list args);
void crashLogSignalSafe(char const* message) noexcept;

} // namespace flux::compositor::diagnostics
