#pragma once

#include <Flux/Core/EventQueue.hpp>

#include <memory>
#include <utility>

#include "../llm-studio/LlamaEngine.hpp"
#include "../llm-studio/ModelManager.hpp"

#include "AppState.hpp"

namespace lambda {

struct BackendServices {
    std::shared_ptr<llm_studio::LlamaEngine> engine;
    std::shared_ptr<llm_studio::ModelManager> manager;
};

inline std::shared_ptr<BackendServices> &backendSlot() {
    static std::shared_ptr<BackendServices> services;
    return services;
}

inline BackendServices &backend() {
    auto &slot = backendSlot();
    if (!slot) {
        slot = std::make_shared<BackendServices>();
        slot->engine = std::make_shared<llm_studio::LlamaEngine>();
        slot->manager = std::make_shared<llm_studio::ModelManager>(
            slot->engine,
            [](ModelManagerEvent ev) {
                flux::Application::instance().eventQueue().post(std::move(ev));
            }
        );
    }
    return *slot;
}

inline void shutdownBackend() {
    auto &slot = backendSlot();
    if (!slot) {
        return;
    }
    slot->manager.reset();
    slot->engine.reset();
    llama_backend_free();
    slot.reset();
}

inline LocalModel toLocalModel(LocalModelInfo const &model) {
    LocalModel local;
    local.path = model.path;
    local.repo = model.repo;
    local.tag = model.tag;
    local.sizeBytes = model.sizeBytes;
    local.name = model.displayName();
    return local;
}

inline ::ChatMessage::Role toBackendRole(ChatRole role) {
    switch (role) {
    case ChatRole::User:
        return ::ChatMessage::Role::User;
    case ChatRole::Reasoning:
        return ::ChatMessage::Role::Reasoning;
    case ChatRole::Assistant:
        return ::ChatMessage::Role::Assistant;
    }
    return ::ChatMessage::Role::Assistant;
}

} // namespace lambda
