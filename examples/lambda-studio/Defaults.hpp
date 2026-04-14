#pragma once

#include <cstdlib>
#include <string>

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

} // namespace lambda_studio_backend
