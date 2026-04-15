#pragma once

#include <cstdlib>
#include <string>

#include "Types.hpp"

namespace lambda_studio_backend {

inline std::string defaultModelPath() {
    if (char const *path = std::getenv("LLAMA_MODEL_PATH")) {
        return std::string(path);
    }
    return {};
}

inline std::string defaultModelName() {
    if (char const *name = std::getenv("LLAMA_MODEL_NAME")) {
        return std::string(name);
    }
    return "local";
}

inline int defaultNGpuLayers() {
    if (char const *value = std::getenv("LLAMA_N_GPU_LAYERS")) {
        return std::atoi(value);
    }
    return -1;
}

inline std::uint32_t defaultNCtx() {
    if (char const *value = std::getenv("LLAMA_N_CTX")) {
        int const parsed = std::atoi(value);
        if (parsed > 0) {
            return static_cast<std::uint32_t>(parsed);
        }
    }
    return 0;
}

inline LoadParams defaultLoadParams(std::string modelPath = defaultModelPath()) {
    return LoadParams {
        .modelPath = std::move(modelPath),
        .nGpuLayers = defaultNGpuLayers(),
        .nCtx = defaultNCtx(),
    };
}

} // namespace lambda_studio_backend
