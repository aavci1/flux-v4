#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace lambda_studio_backend {

inline std::optional<std::filesystem::path> canonicalPathIfExists(std::filesystem::path const &path) {
    std::error_code ec;
    std::filesystem::path const absolute = std::filesystem::absolute(path, ec);
    if (ec) {
        return std::nullopt;
    }
    std::filesystem::path const normalized = absolute.lexically_normal();
    if (!std::filesystem::exists(normalized, ec) || ec) {
        return std::nullopt;
    }
    return normalized;
}

inline bool hasWorkspaceMarker(std::filesystem::path const &path) {
    std::error_code ec;
    return std::filesystem::exists(path / ".git", ec) ||
           std::filesystem::exists(path / ".jj", ec) ||
           std::filesystem::exists(path / ".hg", ec) ||
           std::filesystem::exists(path / "CMakeLists.txt", ec) ||
           std::filesystem::exists(path / "package.json", ec) ||
           std::filesystem::exists(path / "pyproject.toml", ec);
}

inline bool pathLooksLikeBuildOutput(std::filesystem::path const &path) {
    for (std::filesystem::path const &part : path) {
        std::string const component = part.string();
        if (component == "build" || component == "out" ||
            component.rfind("cmake-build-", 0) == 0) {
            return true;
        }
    }
    return false;
}

inline std::optional<std::filesystem::path> discoverWorkspaceRoot(std::filesystem::path start) {
    auto canonicalStart = canonicalPathIfExists(start);
    if (!canonicalStart.has_value()) {
        return std::nullopt;
    }

    std::filesystem::path current = *canonicalStart;
    while (true) {
        if (hasWorkspaceMarker(current)) {
            return current;
        }
        std::filesystem::path const parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    return canonicalStart;
}

inline std::string defaultToolWorkspaceRoot() {
    if (char const *overrideRoot = std::getenv("LAMBDA_STUDIO_TOOL_WORKSPACE_ROOT");
        overrideRoot != nullptr && *overrideRoot != '\0') {
        if (auto canonical = canonicalPathIfExists(overrideRoot); canonical.has_value()) {
            return canonical->string();
        }
        return overrideRoot;
    }

    if (char const *pwd = std::getenv("PWD"); pwd != nullptr && *pwd != '\0') {
        if (auto discovered = discoverWorkspaceRoot(pwd); discovered.has_value()) {
            return discovered->string();
        }
    }

    std::error_code ec;
    std::filesystem::path const cwd = std::filesystem::current_path(ec);
    if (ec) {
        return ".";
    }
    if (auto discovered = discoverWorkspaceRoot(cwd); discovered.has_value()) {
        return discovered->string();
    }
    return cwd.string();
}

inline std::string normalizeToolWorkspaceRoot(std::string workspaceRoot) {
    if (workspaceRoot.empty()) {
        return defaultToolWorkspaceRoot();
    }

    auto normalized = canonicalPathIfExists(workspaceRoot);
    if (!normalized.has_value()) {
        return std::filesystem::path(workspaceRoot).lexically_normal().string();
    }

    if (auto discovered = discoverWorkspaceRoot(*normalized);
        discovered.has_value() && *discovered != *normalized && pathLooksLikeBuildOutput(*normalized)) {
        return discovered->string();
    }

    std::error_code ec;
    std::filesystem::path const cwd = std::filesystem::current_path(ec);
    if (!ec) {
        if (auto canonicalCwd = canonicalPathIfExists(cwd); canonicalCwd.has_value() &&
            *normalized == *canonicalCwd) {
            if (auto discovered = discoverWorkspaceRoot(*canonicalCwd);
                discovered.has_value() && *discovered != *canonicalCwd) {
                return discovered->string();
            }
        }
    }

    return normalized->string();
}

struct ToolCall {
    std::string id;
    std::string name;
    std::string arguments;

    constexpr bool operator==(ToolCall const &) const = default;
};

enum class ToolExecutionState {
    None,
    PendingApproval,
    Running,
    Completed,
    Denied,
    Failed,
};

enum class ShellApprovalMode {
    RequireApproval,
};

struct ToolConfig {
    bool enabled = true;
    std::vector<std::string> enabledToolNames {
        "read_file",
        "file_glob_search",
        "grep_search",
        "exec_shell_command",
    };
    std::string workspaceRoot = defaultToolWorkspaceRoot();
    std::int32_t maxToolCalls = 8;
    ShellApprovalMode shellApprovalMode = ShellApprovalMode::RequireApproval;

    bool operator==(ToolConfig const &) const = default;
};

struct LoadParams {
    std::string modelPath;
    std::int32_t nGpuLayers = -1;
    std::uint32_t nCtx = 0;
    std::uint32_t nBatch = 2048;
    std::uint32_t nUBatch = 512;
    bool useMmap = true;
    bool useMlock = false;
    bool embeddings = false;
    bool offloadKqv = true;
    bool flashAttn = true;

    constexpr bool operator==(LoadParams const &) const = default;
};

struct SessionParams {
    std::uint32_t nCtx = 0;
    std::uint32_t nBatch = 0;
    std::uint32_t nUBatch = 0;
    bool enableThinking = true;
    std::string systemPrompt;
    std::string chatTemplate;
    bool flashAttn = true;
    ToolConfig toolConfig;

    constexpr bool operator==(SessionParams const &) const = default;
};

struct GenerationParams {
    std::uint32_t seed = UINT32_MAX;
    std::int32_t maxTokens = 4096;

    std::int32_t topK = 40;
    float topP = 0.95f;
    float minP = 0.05f;
    float temp = 0.80f;

    std::int32_t penaltyLastN = 64;
    float repeatPenalty = 1.00f;
    float frequencyPenalty = 0.00f;
    float presencePenalty = 0.00f;

    std::int32_t mirostat = 0;
    float mirostatTau = 5.00f;
    float mirostatEta = 0.10f;

    bool ignoreEos = false;

    constexpr bool operator==(GenerationParams const &) const = default;
};

struct LoadParamsPatch {
    std::optional<std::string> modelPath;
    std::optional<std::int32_t> nGpuLayers;
    std::optional<std::uint32_t> nCtx;
    std::optional<std::uint32_t> nBatch;
    std::optional<std::uint32_t> nUBatch;
    std::optional<bool> useMmap;
    std::optional<bool> useMlock;
    std::optional<bool> embeddings;
    std::optional<bool> offloadKqv;
    std::optional<bool> flashAttn;
};

struct SessionParamsPatch {
    std::optional<std::uint32_t> nCtx;
    std::optional<std::uint32_t> nBatch;
    std::optional<std::uint32_t> nUBatch;
    std::optional<bool> enableThinking;
    std::optional<std::string> systemPrompt;
    std::optional<std::string> chatTemplate;
    std::optional<bool> flashAttn;
    std::optional<bool> toolsEnabled;
    std::optional<std::int32_t> maxToolCalls;
    std::optional<std::string> toolWorkspaceRoot;
    std::optional<std::vector<std::string>> enabledToolNames;
};

struct GenerationParamsPatch {
    std::optional<std::uint32_t> seed;
    std::optional<std::int32_t> maxTokens;
    std::optional<std::int32_t> topK;
    std::optional<float> topP;
    std::optional<float> minP;
    std::optional<float> temp;
    std::optional<std::int32_t> penaltyLastN;
    std::optional<float> repeatPenalty;
    std::optional<float> frequencyPenalty;
    std::optional<float> presencePenalty;
    std::optional<std::int32_t> mirostat;
    std::optional<float> mirostatTau;
    std::optional<float> mirostatEta;
    std::optional<bool> ignoreEos;
};

enum class ApplyScope {
    AppliedImmediately,
    Deferred,
    RequiresSessionReset,
    RequiresModelReload,
    Rejected,
};

struct ApplyResult {
    ApplyScope scope = ApplyScope::AppliedImmediately;
    std::string message;
};

struct EngineConfigDefaults {
    LoadParams loadDefaults;
    SessionParams sessionDefaults;
    GenerationParams generationDefaults;

    constexpr bool operator==(EngineConfigDefaults const &) const = default;
};

struct ChatMessage {
    enum class Role {
        User,
        Reasoning,
        Assistant,
        Tool,
    };

    Role role = Role::User;
    std::string text;
    std::vector<ToolCall> toolCalls;
    std::string toolCallId;
    std::string toolName;
    ToolExecutionState toolState = ToolExecutionState::None;

    constexpr bool operator==(ChatMessage const &) const = default;
};

struct ChatGenerationRequest {
    std::string chatId;
    std::uint64_t generationId = 0;
    std::vector<ChatMessage> messages;
    std::optional<GenerationParams> requestGenerationParams;
    std::string summaryText;
    std::size_t summaryMessageCount = 0;
    std::int64_t summaryUpdatedAtUnixMs = 0;

    constexpr bool operator==(ChatGenerationRequest const &) const = default;
};

struct GenerationStats {
    std::int64_t promptTokens = 0;
    std::int64_t completionTokens = 0;
    std::int64_t startedAtUnixMs = 0;
    std::int64_t firstTokenAtUnixMs = 0;
    std::int64_t finishedAtUnixMs = 0;
    double tokensPerSecond = 0.0;
    std::string status;
    std::string errorText;
    std::uint32_t seed = UINT32_MAX;
    float temp = 0.f;
    std::int32_t topK = 0;
    float topP = 0.f;
    float minP = 0.f;
    std::int32_t maxTokens = 0;
    std::int32_t penaltyLastN = 0;
    float repeatPenalty = 0.f;
    float frequencyPenalty = 0.f;
    float presencePenalty = 0.f;
    std::int32_t mirostat = 0;
    float mirostatTau = 0.f;
    float mirostatEta = 0.f;
    bool ignoreEos = false;

    constexpr bool operator==(GenerationStats const &) const = default;
};

struct LlmUiEvent {
    enum class Kind {
        Chunk,
        ToolCallsParsed,
        ToolApprovalRequested,
        ToolStarted,
        ToolFinished,
        ToolDenied,
        ToolFailed,
        Done,
        Error,
    };
    enum class Part {
        Content,
        Thinking,
    };

    Kind kind = Kind::Done;
    Part part = Part::Content;
    std::string chatId;
    std::uint64_t generationId = 0;
    std::string text;
    std::vector<ToolCall> toolCalls;
    ToolCall toolCall;
    std::string toolCallId;
    std::string toolName;
    ToolExecutionState toolState = ToolExecutionState::None;
    std::string summaryText;
    std::size_t summaryMessageCount = 0;
    std::int64_t summaryUpdatedAtUnixMs = 0;
    std::optional<GenerationStats> generationStats;
};

struct LocalModelInfo {
    std::string repo;
    std::string tag;
    std::string path;
    std::size_t sizeBytes = 0;

    std::string displayName() const {
        if (!path.empty()) {
            std::filesystem::path filePath(path);
            std::string name = filePath.filename().string();
            if (name.size() > 5 && name.substr(name.size() - 5) == ".gguf") {
                name.resize(name.size() - 5);
            }
            if (!name.empty()) {
                return name;
            }
        }
        if (!repo.empty()) {
            return tag.empty() ? repo : repo + ":" + tag;
        }
        auto const pos = path.rfind('/');
        return pos != std::string::npos ? path.substr(pos + 1) : path;
    }

    constexpr bool operator==(LocalModelInfo const &) const = default;
};

struct HfModelInfo {
    std::string id;
    std::string author;
    std::string libraryName;
    std::string lastModified;
    std::string createdAt;
    std::int64_t downloads = 0;
    std::int64_t downloadsAllTime = 0;
    std::int64_t likes = 0;
    std::int64_t usedStorage = 0;
    std::string pipelineTag;
    std::vector<std::string> tags;
    bool gated = false;
    bool isPrivate = false;
    bool disabled = false;

    constexpr bool operator==(HfModelInfo const &) const = default;
};

struct HfSearchRequest {
    std::string query;
    std::string sortKey = "downloads";
    std::string visibilityFilter = "all";
    std::string cacheKey;

    constexpr bool operator==(HfSearchRequest const &) const = default;
};

struct HfFileInfo {
    std::string repoId;
    std::string path;
    std::size_t sizeBytes = 0;
    std::string localPath;
    bool cached = false;

    constexpr bool operator==(HfFileInfo const &) const = default;
};

struct HfRepoDetailInfo {
    std::string id;
    std::string author;
    std::string sha;
    std::string libraryName;
    std::string pipelineTag;
    std::string license;
    std::string summary;
    std::string readme;
    std::string createdAt;
    std::string lastModified;
    std::vector<std::string> tags;
    std::vector<std::string> languages;
    std::vector<std::string> baseModels;
    std::int64_t downloads = 0;
    std::int64_t downloadsAllTime = 0;
    std::int64_t likes = 0;
    std::int64_t usedStorage = 0;
    bool gated = false;
    bool isPrivate = false;
    bool disabled = false;

    constexpr bool operator==(HfRepoDetailInfo const &) const = default;
};

enum class ModelManagerLane {
    Inventory,
    Search,
    RepoInspect,
    Download,
    LoadModel,
};

struct ModelManagerEvent {
    enum class Kind {
        LocalModelsReady,
        HfSearchReady,
        HfFilesReady,
        HfRepoDetailReady,
        DownloadProgress,
        DownloadDone,
        DownloadError,
        ModelLoaded,
        ModelLoadError,
        ModelDeleted,
        ModelDeleteError,
    };

    Kind kind = Kind::LocalModelsReady;
    std::vector<LocalModelInfo> localModels;
    std::vector<HfModelInfo> hfModels;
    std::vector<HfFileInfo> hfFiles;
    HfRepoDetailInfo hfRepoDetail;
    std::string error;
    std::string searchKey;
    std::string repoId;
    std::string filePath;
    std::string modelPath;
    std::string modelName;
    std::string rawJson;
    std::size_t downloadedBytes = 0;
    std::size_t totalBytes = 0;
    LoadParams appliedLoadParams;
    std::uint64_t requestId = 0;
    ModelManagerLane lane = ModelManagerLane::Inventory;
};

} // namespace lambda_studio_backend
