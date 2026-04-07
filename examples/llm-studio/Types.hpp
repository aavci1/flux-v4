#pragma once

#include <array>
#include <cstddef>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>

/// RFC 4122 UUID version 4 (random), lowercase hex with dashes.
inline std::string generateChatId() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<unsigned> byteDis(0, 255);
  std::array<unsigned char, 16> bytes {};
  for (auto& b : bytes) {
    b = static_cast<unsigned char>(byteDis(gen));
  }
  bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0fu) | 0x40u);
  bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3fu) | 0x80u);

  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (size_t i = 0; i < 16; ++i) {
    oss << std::setw(2) << static_cast<unsigned>(bytes[i]);
    if (i == 3 || i == 5 || i == 7 || i == 9) {
      oss << '-';
    }
  }
  return oss.str();
}

struct ChatMessage {
    enum class Role { User, Reasoning, Assistant };

    Role role = Role::User;
    std::string text;

    constexpr bool operator==(ChatMessage const& o) const = default;
};

struct Chat {
    std::string modelName {};
    std::string id {};
    std::string title {""};
    std::vector<ChatMessage> messages {};
    bool streaming = false;

    constexpr bool operator==(Chat const& o) const = default;
};

// ── Model management types ──────────────────────────────────────────────────

struct LocalModelInfo {
    std::string repo;
    std::string tag;
    std::string path;
    size_t      sizeBytes = 0;

    std::string displayName() const {
        if (!repo.empty()) {
            return tag.empty() ? repo : repo + ":" + tag;
        }
        auto pos = path.rfind('/');
        return (pos != std::string::npos) ? path.substr(pos + 1) : path;
    }

    bool operator==(LocalModelInfo const& o) const = default;
};

struct HfModelInfo {
    std::string id;
    int64_t     downloads = 0;
    int64_t     likes     = 0;
    std::string pipelineTag;

    bool operator==(HfModelInfo const& o) const = default;
};

struct HfFileInfo {
    std::string repoId;
    std::string path;
    size_t      sizeBytes = 0;

    bool operator==(HfFileInfo const& o) const = default;
};

struct ModelManagerEvent {
    enum class Kind {
        LocalModelsReady,
        HfSearchReady,
        HfFilesReady,
        DownloadDone,
        DownloadError,
        ModelLoaded,
        ModelLoadError,
    };

    Kind kind = Kind::LocalModelsReady;

    std::vector<LocalModelInfo> localModels;
    std::vector<HfModelInfo>    hfModels;
    std::vector<HfFileInfo>     hfFiles;
    std::string                 error;
    std::string                 modelPath;
    std::string                 modelName;
};

struct SamplingParams {
    float   temp      = 0.80f;
    float   topP      = 0.95f;
    int32_t topK      = 40;
    int32_t maxTokens = 4096;
    int32_t nGpuLayers = -1;

    bool operator==(SamplingParams const& o) const = default;
};