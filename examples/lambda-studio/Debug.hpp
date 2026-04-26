#pragma once

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace lambda_studio_backend {

namespace detail {

inline bool envFlagEnabled(char const *name) {
    char const *value = std::getenv(name);
    return value && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

inline int envIntOr(char const *name, int fallback) {
    char const *value = std::getenv(name);
    if (!value || value[0] == '\0') {
        return fallback;
    }
    return std::max(1, std::atoi(value));
}

} // namespace detail

inline bool debugFakeStreamEnabled() {
    return detail::envFlagEnabled("LAMBDA_STUDIO_FAKE_STREAM");
}

inline bool debugAutoStreamEnabled() {
    return detail::envFlagEnabled("LAMBDA_STUDIO_AUTO_STREAM");
}

inline int debugFakeTokensPerSecond() {
    return detail::envIntOr("LAMBDA_STUDIO_FAKE_TOKENS_PER_SECOND", 10);
}

inline bool debugFakeToolApprovalEnabled() {
    return detail::envFlagEnabled("LAMBDA_STUDIO_FAKE_TOOL_APPROVAL");
}

inline bool debugToolTraceEnabled() {
    return detail::envFlagEnabled("LAMBDA_STUDIO_TOOL_TRACE");
}

inline std::string debugToolTraceLogPath() {
    if (char const *path = std::getenv("LAMBDA_STUDIO_TOOL_TRACE_LOG"); path != nullptr && *path != '\0') {
        return path;
    }
    return "/tmp/lambda_studio_tool_trace.log";
}

inline void debugToolTrace(std::string_view message) {
    if (!debugToolTraceEnabled()) {
        return;
    }

    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);

    std::string line = std::string("[lambda-tool-trace] ") + std::string(message) + "\n";
    std::fwrite(line.data(), 1, line.size(), stderr);
    std::fflush(stderr);

    std::filesystem::path const path(debugToolTraceLogPath());
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    std::ofstream stream(path, std::ios::app);
    if (stream) {
        stream << line;
        stream.flush();
    }
}

inline std::string debugFakeMarkdownResponse() {
    return R"(# Streaming Markdown Stress Test

## Overview

This response is intentionally long so we can observe **CPU usage** while the UI receives markdown over time.

- The renderer should keep old content stable.
- Only the newest chunk should be changing.
- Inline `code` and headings are included on purpose.

### Checklist

- Rebuild the attributed markdown only when needed.
- Avoid expensive work for messages that are only being scrolled.
- Keep the chat responsive while the stream is active.

```cpp
for (int frame = 0; frame < 600; ++frame) {
    render_chat_transcript();
    sample_cpu_usage();
}
```

## Notes

The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog.

The UI should render **bold emphasis**, keep `inline spans` readable, and avoid chewing CPU once the message is complete.

- Item one explains the cost of parsing.
- Item two explains the cost of text layout.
- Item three explains the cost of rebuilding the whole transcript.

### Closing

If this fake stream still drives high CPU after the response settles, the hotspot is probably outside the markdown parser and closer to layout or transcript rebuilding.)";
}

} // namespace lambda_studio_backend
