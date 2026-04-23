#pragma once

#include "Debug.hpp"
#include "Types.hpp"

#include "chat.h"
#include "common.h"

#include <nlohmann/json.hpp>
#include <sheredom/subprocess.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace lambda_studio_backend {

namespace tooling {

using json = nlohmann::ordered_json;
namespace fs = std::filesystem;

static constexpr std::size_t kMaxFileReadBytes = 16 * 1024;
static constexpr std::size_t kMaxSearchResults = 100;
static constexpr std::size_t kMaxShellOutputBytes = 16 * 1024;
static constexpr int kDefaultShellTimeoutSeconds = 10;
static constexpr int kMaxShellTimeoutSeconds = 60;

struct RegisteredTool {
    common_chat_tool schema;
    bool requiresApproval = false;
};

inline std::vector<char *> toCStringVector(std::vector<std::string> const &values) {
    std::vector<char *> result;
    result.reserve(values.size() + 1);
    for (auto const &value : values) {
        result.push_back(const_cast<char *>(value.c_str()));
    }
    result.push_back(nullptr);
    return result;
}

struct RunProcessResult {
    std::string output;
    int exitCode = -1;
    bool timedOut = false;
};

inline RunProcessResult runProcess(
    std::vector<std::string> const &args,
    std::size_t maxOutputBytes,
    int timeoutSeconds
) {
    RunProcessResult result;

    subprocess_s proc;
    auto argv = toCStringVector(args);
    int const options = subprocess_option_no_window |
                        subprocess_option_combined_stdout_stderr |
                        subprocess_option_inherit_environment |
                        subprocess_option_search_user_path;

    if (subprocess_create(argv.data(), options, &proc) != 0) {
        result.output = "failed to spawn process";
        return result;
    }

    std::atomic<bool> done {false};
    std::atomic<bool> timedOut {false};
    std::thread timeoutThread([&]() {
        auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
        while (!done.load(std::memory_order_relaxed)) {
            if (std::chrono::steady_clock::now() >= deadline) {
                timedOut.store(true, std::memory_order_relaxed);
                subprocess_terminate(&proc);
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    std::string output;
    bool truncated = false;
    FILE *pipe = subprocess_stdout(&proc);
    if (pipe != nullptr) {
        char buffer[4096];
        while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            if (truncated) {
                continue;
            }
            std::size_t const len = std::strlen(buffer);
            if (output.size() + len <= maxOutputBytes) {
                output.append(buffer, len);
            } else {
                output.append(buffer, maxOutputBytes - output.size());
                truncated = true;
            }
        }
    }

    done.store(true, std::memory_order_relaxed);
    if (timeoutThread.joinable()) {
        timeoutThread.join();
    }

    subprocess_join(&proc, &result.exitCode);
    subprocess_destroy(&proc);

    result.output = std::move(output);
    result.timedOut = timedOut.load(std::memory_order_relaxed);
    if (truncated) {
        result.output += "\n[output truncated]";
    }
    return result;
}

inline json safeParseArguments(std::string const &raw) {
    if (raw.empty()) {
        return json::object();
    }
    json parsed = json::parse(raw, nullptr, false);
    if (parsed.is_discarded()) {
        return json::object({{"_raw", raw}});
    }
    return parsed;
}

inline bool isSubpath(fs::path const &path, fs::path const &root) {
    auto const pathStr = path.lexically_normal().string();
    auto const rootStr = root.lexically_normal().string();
    if (pathStr == rootStr) {
        return true;
    }
    if (pathStr.size() <= rootStr.size()) {
        return false;
    }
    if (pathStr.compare(0, rootStr.size(), rootStr) != 0) {
        return false;
    }
    char const next = pathStr[rootStr.size()];
    return next == '/' || next == '\\';
}

inline std::optional<fs::path> resolvePathWithinRoot(std::string const &rawPath, std::string const &workspaceRoot) {
    std::error_code ec;
    std::string const normalizedRoot = normalizeToolWorkspaceRoot(workspaceRoot.empty() ? "." : workspaceRoot);
    fs::path const root = fs::weakly_canonical(fs::path(normalizedRoot), ec);
    if (ec) {
        debugToolTrace(
            string_format(
                "resolvePathWithinRoot root_error raw_path=%s workspace_root=%s normalized_root=%s error=%s",
                rawPath.c_str(),
                workspaceRoot.c_str(),
                normalizedRoot.c_str(),
                ec.message().c_str()
            )
        );
        return std::nullopt;
    }

    fs::path path = fs::path(rawPath);
    if (path.is_relative()) {
        path = root / path;
    }
    path = fs::weakly_canonical(path, ec);
    if (ec || !isSubpath(path, root)) {
        debugToolTrace(
            string_format(
                "resolvePathWithinRoot rejected raw_path=%s workspace_root=%s normalized_root=%s resolved=%s error=%s",
                rawPath.c_str(),
                workspaceRoot.c_str(),
                normalizedRoot.c_str(),
                path.string().c_str(),
                ec ? ec.message().c_str() : ""
            )
        );
        return std::nullopt;
    }
    debugToolTrace(
        string_format(
            "resolvePathWithinRoot accepted raw_path=%s workspace_root=%s normalized_root=%s resolved=%s",
            rawPath.c_str(),
            workspaceRoot.c_str(),
            normalizedRoot.c_str(),
            path.string().c_str()
        )
    );
    return path;
}

inline json errorResult(std::string toolName, std::string message) {
    return json {
        {"ok", false},
        {"tool", std::move(toolName)},
        {"error", std::move(message)},
    };
}

inline json readFileTool(json const &params, ToolConfig const &config) {
    std::string const rawPath = params.value("path", std::string());
    int const startLine = params.value("start_line", 1);
    int const endLine = params.value("end_line", -1);
    bool const appendLoc = params.value("append_loc", false);

    auto path = resolvePathWithinRoot(rawPath, config.workspaceRoot);
    if (!path.has_value()) {
        return errorResult("read_file", "path is outside the configured workspace root");
    }

    std::error_code ec;
    std::uintmax_t const fileSize = fs::file_size(*path, ec);
    if (ec) {
        return errorResult("read_file", "cannot stat file: " + ec.message());
    }
    if (fileSize > kMaxFileReadBytes && endLine == -1) {
        return errorResult(
            "read_file",
            string_format(
                "file too large (%zu bytes, max %zu). Use start_line/end_line.",
                static_cast<std::size_t>(fileSize),
                kMaxFileReadBytes
            )
        );
    }

    std::ifstream stream(path->string());
    if (!stream) {
        return errorResult("read_file", "failed to open file");
    }

    std::string output;
    std::string line;
    int lineNo = 0;
    while (std::getline(stream, line)) {
        ++lineNo;
        if (lineNo < startLine) {
            continue;
        }
        if (endLine != -1 && lineNo > endLine) {
            break;
        }
        std::string rendered = appendLoc ? std::to_string(lineNo) + "-> " + line + "\n" : line + "\n";
        if (output.size() + rendered.size() > kMaxFileReadBytes) {
            output += "[output truncated]";
            break;
        }
        output += rendered;
    }

    return json {
        {"ok", true},
        {"tool", "read_file"},
        {"path", path->string()},
        {"plain_text_response", output},
    };
}

inline json fileGlobSearchTool(json const &params, ToolConfig const &config) {
    std::string const rawPath = params.value("path", std::string());
    std::string const include = params.value("include", std::string("**"));
    std::string const exclude = params.value("exclude", std::string());

    auto base = resolvePathWithinRoot(rawPath, config.workspaceRoot);
    if (!base.has_value()) {
        return errorResult("file_glob_search", "path is outside the configured workspace root");
    }

    std::ostringstream output;
    std::size_t count = 0;
    std::error_code ec;
    for (auto const &entry : fs::recursive_directory_iterator(
             *base,
             fs::directory_options::skip_permission_denied,
             ec
         )) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        std::string rel = fs::relative(entry.path(), *base, ec).string();
        if (ec) {
            continue;
        }
        std::replace(rel.begin(), rel.end(), '\\', '/');
        if (!glob_match(include, rel)) {
            continue;
        }
        if (!exclude.empty() && glob_match(exclude, rel)) {
            continue;
        }
        output << entry.path().string() << "\n";
        if (++count >= kMaxSearchResults) {
            break;
        }
    }
    output << "\n---\nTotal matches: " << count << "\n";

    return json {
        {"ok", true},
        {"tool", "file_glob_search"},
        {"path", base->string()},
        {"plain_text_response", output.str()},
    };
}

inline json grepSearchTool(json const &params, ToolConfig const &config) {
    std::string const rawPath = params.value("path", std::string());
    std::string const patternValue = params.value("pattern", std::string());
    std::string const include = params.value("include", std::string("**"));
    std::string const exclude = params.value("exclude", std::string());
    bool const showLineNumbers = params.value("return_line_numbers", false);

    auto path = resolvePathWithinRoot(rawPath, config.workspaceRoot);
    if (!path.has_value()) {
        return errorResult("grep_search", "path is outside the configured workspace root");
    }

    std::regex pattern;
    try {
        pattern = std::regex(patternValue);
    } catch (std::regex_error const &e) {
        return errorResult("grep_search", std::string("invalid regex: ") + e.what());
    }

    std::ostringstream output;
    std::size_t total = 0;
    auto searchFile = [&](fs::path const &filePath) {
        std::ifstream stream(filePath.string());
        if (!stream) {
            return;
        }
        std::string line;
        int lineNo = 0;
        while (std::getline(stream, line) && total < kMaxSearchResults) {
            ++lineNo;
            if (!std::regex_search(line, pattern)) {
                continue;
            }
            output << filePath.string() << ":";
            if (showLineNumbers) {
                output << lineNo << ":";
            }
            output << line << "\n";
            ++total;
        }
    };

    std::error_code ec;
    if (fs::is_regular_file(*path, ec)) {
        searchFile(*path);
    } else if (fs::is_directory(*path, ec)) {
        for (auto const &entry : fs::recursive_directory_iterator(
                 *path,
                 fs::directory_options::skip_permission_denied,
                 ec
             )) {
            if (ec || !entry.is_regular_file() || total >= kMaxSearchResults) {
                continue;
            }
            std::string rel = fs::relative(entry.path(), *path, ec).string();
            if (ec) {
                continue;
            }
            std::replace(rel.begin(), rel.end(), '\\', '/');
            if (!glob_match(include, rel)) {
                continue;
            }
            if (!exclude.empty() && glob_match(exclude, rel)) {
                continue;
            }
            searchFile(entry.path());
        }
    } else {
        return errorResult("grep_search", "path does not exist");
    }

    output << "\n---\nTotal matches: " << total << "\n";
    return json {
        {"ok", true},
        {"tool", "grep_search"},
        {"path", path->string()},
        {"plain_text_response", output.str()},
    };
}

inline std::string shellQuote(std::string const &value) {
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted += "'";
    return quoted;
}

inline json execShellCommandTool(json const &params, ToolConfig const &config) {
    std::string const command = params.value("command", std::string());
    int timeout = params.value("timeout", kDefaultShellTimeoutSeconds);
    std::size_t maxOutputSize = static_cast<std::size_t>(params.value(
        "max_output_size",
        static_cast<int>(kMaxShellOutputBytes)
    ));

    timeout = std::min(timeout, kMaxShellTimeoutSeconds);
    maxOutputSize = std::min(maxOutputSize, kMaxShellOutputBytes);

    std::string const workspaceRoot = normalizeToolWorkspaceRoot(config.workspaceRoot);
    std::string const rootedCommand =
        "cd " + shellQuote(workspaceRoot.empty() ? "." : workspaceRoot) + " && " + command;

#ifdef _WIN32
    std::vector<std::string> args = {"cmd", "/c", rootedCommand};
#else
    std::vector<std::string> args = {"sh", "-lc", rootedCommand};
#endif

    RunProcessResult const run = runProcess(args, maxOutputSize, timeout);
    return json {
        {"ok", run.exitCode == 0 && !run.timedOut},
        {"tool", "exec_shell_command"},
        {"command", command},
        {"workspace_root", workspaceRoot},
        {"output", run.output},
        {"plain_text_response", run.output},
        {"exit_code", run.exitCode},
        {"timed_out", run.timedOut},
        {"error", run.exitCode == 0 && !run.timedOut ? std::string() :
                  (run.timedOut ? std::string("command timed out") : std::string("command exited with non-zero status"))},
    };
}

inline std::optional<RegisteredTool> registeredToolByName(std::string const &name) {
    if (name == "read_file") {
        return RegisteredTool {
            .schema = common_chat_tool {
                .name = "read_file",
                .description = "Read the contents of a file inside the workspace. Optionally specify a 1-based line range.",
                .parameters = json {
                    {"type", "object"},
                    {"properties", {
                        {"path", {{"type", "string"}, {"description", "Path to the file"}}},
                        {"start_line", {{"type", "integer"}, {"description", "First line to read, 1-based (default: 1)"}}},
                        {"end_line", {{"type", "integer"}, {"description", "Last line to read, 1-based inclusive (default: end of file)"}}},
                        {"append_loc", {{"type", "boolean"}, {"description", "Prefix each line with its line number"}}},
                    }},
                    {"required", json::array({"path"})},
                }.dump(),
            },
            .requiresApproval = false,
        };
    }
    if (name == "file_glob_search") {
        return RegisteredTool {
            .schema = common_chat_tool {
                .name = "file_glob_search",
                .description = "Recursively search for files matching a glob pattern under a workspace directory.",
                .parameters = json {
                    {"type", "object"},
                    {"properties", {
                        {"path", {{"type", "string"}, {"description", "Base directory to search in"}}},
                        {"include", {{"type", "string"}, {"description", "Glob pattern for files to include (default: **)"}}},
                        {"exclude", {{"type", "string"}, {"description", "Glob pattern for files to exclude"}}},
                    }},
                    {"required", json::array({"path"})},
                }.dump(),
            },
            .requiresApproval = false,
        };
    }
    if (name == "grep_search") {
        return RegisteredTool {
            .schema = common_chat_tool {
                .name = "grep_search",
                .description = "Search for a regex pattern in files under a workspace path and return matching lines.",
                .parameters = json {
                    {"type", "object"},
                    {"properties", {
                        {"path", {{"type", "string"}, {"description", "File or directory to search in"}}},
                        {"pattern", {{"type", "string"}, {"description", "Regular expression pattern to search for"}}},
                        {"include", {{"type", "string"}, {"description", "Glob pattern to filter files (default: **)"}}},
                        {"exclude", {{"type", "string"}, {"description", "Glob pattern to exclude files"}}},
                        {"return_line_numbers", {{"type", "boolean"}, {"description", "Include line numbers in results"}}},
                    }},
                    {"required", json::array({"path", "pattern"})},
                }.dump(),
            },
            .requiresApproval = false,
        };
    }
    if (name == "exec_shell_command") {
        return RegisteredTool {
            .schema = common_chat_tool {
                .name = "exec_shell_command",
                .description = "Execute a shell command inside the workspace root and return combined stdout/stderr.",
                .parameters = json {
                    {"type", "object"},
                    {"properties", {
                        {"command", {{"type", "string"}, {"description", "Shell command to execute"}}},
                        {"timeout", {{"type", "integer"}, {"description", string_format("Timeout in seconds (default 10, max %d)", kMaxShellTimeoutSeconds)}}},
                        {"max_output_size", {{"type", "integer"}, {"description", string_format("Maximum output size in bytes (default %zu)", kMaxShellOutputBytes)}}},
                    }},
                    {"required", json::array({"command"})},
                }.dump(),
            },
            .requiresApproval = true,
        };
    }
    return std::nullopt;
}

inline std::vector<RegisteredTool> buildRegisteredTools(ToolConfig const &config) {
    std::vector<RegisteredTool> result;
    if (!config.enabled) {
        return result;
    }
    result.reserve(config.enabledToolNames.size());
    for (auto const &name : config.enabledToolNames) {
        auto registered = registeredToolByName(name);
        if (registered.has_value()) {
            result.push_back(std::move(*registered));
        }
    }
    return result;
}

inline std::vector<common_chat_tool> buildChatTools(ToolConfig const &config) {
    std::vector<common_chat_tool> tools;
    for (auto const &registered : buildRegisteredTools(config)) {
        tools.push_back(registered.schema);
    }
    return tools;
}

inline bool requiresApproval(ToolConfig const &config, std::string const &toolName) {
    for (auto const &registered : buildRegisteredTools(config)) {
        if (registered.schema.name == toolName) {
            return registered.requiresApproval;
        }
    }
    return false;
}

inline json invokeTool(
    ToolConfig const &config,
    std::string const &toolName,
    std::string const &arguments
) {
    debugToolTrace(
        string_format(
            "invokeTool name=%s workspace_root=%s args=%s",
            toolName.c_str(),
            config.workspaceRoot.c_str(),
            arguments.substr(0, 512).c_str()
        )
    );
    json params = safeParseArguments(arguments);
    if (toolName == "read_file") {
        return readFileTool(params, config);
    }
    if (toolName == "file_glob_search") {
        return fileGlobSearchTool(params, config);
    }
    if (toolName == "grep_search") {
        return grepSearchTool(params, config);
    }
    if (toolName == "exec_shell_command") {
        return execShellCommandTool(params, config);
    }
    return errorResult(toolName, "tool is not enabled");
}

inline std::string formatToolResult(json const &result) {
    return result.dump();
}

inline std::string toolFormatLabel(common_chat_format format) {
    switch (format) {
    case COMMON_CHAT_FORMAT_PEG_NATIVE:
        return "Native";
    case COMMON_CHAT_FORMAT_PEG_SIMPLE:
    case COMMON_CHAT_FORMAT_PEG_GEMMA4:
        return "Generic";
    case COMMON_CHAT_FORMAT_CONTENT_ONLY:
    case COMMON_CHAT_FORMAT_COUNT:
        break;
    }
    return "Unavailable";
}

} // namespace tooling

} // namespace lambda_studio_backend
