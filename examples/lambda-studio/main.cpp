#include <Flux.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "AppState.hpp"
#include "Debug.hpp"
#include "Defaults.hpp"
#include "Mappers.hpp"
#include "AppRuntime.hpp"
#include "ChatsView.hpp"
#include "HubView.hpp"
#include "ModelsView.hpp"
#include "SettingsView.hpp"
#include "Sidebar.hpp"

#include "common.h"

using namespace flux;
using namespace lambda;

namespace {
std::int64_t steadyNowNanos() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

std::string titleFromPrompt(std::string const &prompt);
lambda_studio_backend::ChatGenerationRequest toGenerationRequest(ChatThread const &chat);

std::string selectedChatIdForState(AppState const &state) {
    int const selectedIndex = clampedChatIndex(state);
    if (selectedIndex < 0) {
        return {};
    }
    return state.chats[static_cast<std::size_t>(selectedIndex)].id;
}

void restoreSelectedChat(AppState &state, std::string const &selectedChatId) {
    if (state.chats.empty()) {
        state.selectedChatIndex = 0;
        return;
    }

    auto const it = std::find_if(state.chats.begin(), state.chats.end(), [&](ChatThread const &chat) {
        return chat.id == selectedChatId;
    });
    if (it != state.chats.end()) {
        state.selectedChatIndex = static_cast<int>(std::distance(state.chats.begin(), it));
        return;
    }
    state.selectedChatIndex = 0;
}

std::vector<ChatMessage> storedChatMessages(ChatThread const &thread) {
    std::vector<ChatMessage> messages = thread.messages;
    messages.reserve(thread.messages.size() + thread.streamDraftMessages.size());
    for (ChatMessage const &draft : thread.streamDraftMessages) {
        if (draft.text.empty() && draft.toolCalls.empty() && draft.role != ChatRole::Tool) {
            continue;
        }
        ChatMessage stored = draft;
        syncAssistantParagraphs(stored);
        messages.push_back(std::move(stored));
    }
    return messages;
}

int chatIndexById(AppState const &state, std::string const &chatId) {
    auto const it = std::find_if(state.chats.begin(), state.chats.end(), [&](ChatThread const &chat) {
        return chat.id == chatId;
    });
    if (it == state.chats.end()) {
        return -1;
    }
    return static_cast<int>(std::distance(state.chats.begin(), it));
}

void persistSelectedChat(AppState &state, IStore &catalog) {
    try {
        catalog.updateSelectedChatId(selectedChatIdForState(state));
    } catch (std::exception const &e) {
        state.errorText = e.what();
    }
}

void persistChatOrder(AppState &state, IStore &catalog) {
    try {
        std::vector<std::string> chatIds;
        chatIds.reserve(state.chats.size());
        for (ChatThread const &chat : state.chats) {
            chatIds.push_back(chat.id);
        }
        catalog.replaceChatOrder(chatIds);
    } catch (std::exception const &e) {
        state.errorText = e.what();
    }
}

void persistChatThread(
    AppState &state,
    IStore &catalog,
    int chatIndex,
    bool persistSelection = false
) {
    if (chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= state.chats.size()) {
        return;
    }
    ChatThread const &chat = state.chats[static_cast<std::size_t>(chatIndex)];
    try {
        catalog.upsertChatThreadMeta(
            chat.id,
            chat.title,
            chat.updatedAtUnixMs,
            chat.modelPath,
            chat.modelName,
            chat.summaryText,
            chat.summaryMessageCount,
            chat.summaryUpdatedAtUnixMs,
            chatIndex
        );
        catalog.updateChatThreadGenerationDefaults(chat.id, chat.generationDefaults);
        catalog.replaceChatMessagesForThread(chat.id, storedChatMessages(chat));
        if (persistSelection) {
            catalog.updateSelectedChatId(selectedChatIdForState(state));
        }
    } catch (std::exception const &e) {
        state.errorText = e.what();
    }
}

void removeDownloadJobFromState(AppState &state, std::string const &jobId) {
    std::erase_if(state.recentDownloadJobs, [&](DownloadJob const &job) {
        return job.id == jobId;
    });
}

void removeDownloadJobsForArtifact(AppState &state, std::string const &repoId, std::string const &filePath) {
    std::erase_if(state.recentDownloadJobs, [&](DownloadJob const &job) {
        return job.repoId == repoId && job.filePath == filePath;
    });
}

MessageGenerationStats toMessageGenerationStats(
    lambda_studio_backend::GenerationStats const &stats,
    ChatThread const &chat
) {
    return MessageGenerationStats {
        .modelPath = chat.modelPath,
        .modelName = chat.modelName,
        .promptTokens = stats.promptTokens,
        .completionTokens = stats.completionTokens,
        .startedAtUnixMs = stats.startedAtUnixMs,
        .firstTokenAtUnixMs = stats.firstTokenAtUnixMs,
        .finishedAtUnixMs = stats.finishedAtUnixMs,
        .tokensPerSecond = stats.tokensPerSecond,
        .status = stats.status,
        .errorText = stats.errorText,
        .seed = stats.seed,
        .temp = stats.temp,
        .topP = stats.topP,
        .topK = stats.topK,
        .minP = stats.minP,
        .maxTokens = stats.maxTokens,
        .penaltyLastN = stats.penaltyLastN,
        .repeatPenalty = stats.repeatPenalty,
        .frequencyPenalty = stats.frequencyPenalty,
        .presencePenalty = stats.presencePenalty,
        .mirostat = stats.mirostat,
        .mirostatTau = stats.mirostatTau,
        .mirostatEta = stats.mirostatEta,
        .ignoreEos = stats.ignoreEos,
    };
}

void attachGenerationStatsToLatestResponseMessages(
    ChatThread &chat,
    lambda_studio_backend::GenerationStats const &stats
) {
    MessageGenerationStats const messageStats = toMessageGenerationStats(stats, chat);
    std::size_t responseStart = chat.messages.size();
    while (responseStart > 0) {
        if (chat.messages[responseStart - 1].role == ChatRole::User) {
            break;
        }
        --responseStart;
    }

    ChatMessage *latestAssistant = nullptr;
    ChatMessage *latestReasoning = nullptr;
    for (std::size_t i = responseStart; i < chat.messages.size(); ++i) {
        ChatMessage &message = chat.messages[i];
        if (message.role == ChatRole::Assistant) {
            latestAssistant = &message;
        } else if (message.role == ChatRole::Reasoning) {
            latestReasoning = &message;
        }
    }

    if (latestReasoning != nullptr) {
        latestReasoning->generationStats = messageStats;
    }
    if (latestAssistant != nullptr) {
        latestAssistant->generationStats = messageStats;
    }
}

void finishTrailingReasoningMessage(std::vector<ChatMessage> &messages, std::int64_t finishedAtNanos) {
    if (messages.empty()) {
        return;
    }
    lambda::ChatMessage &last = messages.back();
    if (last.role == ChatRole::Reasoning && last.finishedAtNanos == 0) {
        last.finishedAtNanos = finishedAtNanos;
    }
}

void finishTrailingReasoningMessage(ChatThread &chat, std::int64_t finishedAtNanos) {
    finishTrailingReasoningMessage(chat.messages, finishedAtNanos);
}

ChatMessage *chatMessageByIndex(ChatThread &chat, std::size_t index) {
    if (index < chat.messages.size()) {
        return &chat.messages[index];
    }
    std::size_t const draftIndex = index - chat.messages.size();
    if (draftIndex < chat.streamDraftMessages.size()) {
        return &chat.streamDraftMessages[draftIndex];
    }
    return nullptr;
}

void commitStreamDraft(ChatThread &chat, std::int64_t finishedAtNanos) {
    finishTrailingReasoningMessage(chat.streamDraftMessages, finishedAtNanos);
    for (ChatMessage &message : chat.streamDraftMessages) {
        if (message.text.empty() && message.toolCalls.empty() && message.role != ChatRole::Tool) {
            continue;
        }
        syncAssistantParagraphs(message);
        chat.messages.push_back(std::move(message));
    }
    chat.streamDraftMessages.clear();
}

void expirePendingToolApprovals(ChatThread &chat) {
    auto expireMessage = [](ChatMessage &message) {
        if (message.role != ChatRole::Tool || message.toolState != ToolMessageState::PendingApproval) {
            return;
        }
        message.toolState = ToolMessageState::Failed;
        if (message.text.empty()) {
            message.text = "{\"ok\":false,\"error\":\"pending tool approval expired after restart\"}";
            ++message.textRevision;
        }
    };

    for (ChatMessage &message : chat.messages) {
        expireMessage(message);
    }
    for (ChatMessage &message : chat.streamDraftMessages) {
        expireMessage(message);
    }
}

ChatMessage *latestAssistantDraft(std::vector<ChatMessage> &messages) {
    for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
        if (it->role == ChatRole::Assistant) {
            return &(*it);
        }
        if (it->role == ChatRole::User) {
            break;
        }
    }
    return nullptr;
}

ChatMessage &ensureAssistantDraftForToolCalls(ChatThread &chat) {
    if (ChatMessage *existing = latestAssistantDraft(chat.streamDraftMessages)) {
        return *existing;
    }
    chat.streamDraftMessages.push_back(ChatMessage {
        .role = ChatRole::Assistant,
        .text = "",
    });
    return chat.streamDraftMessages.back();
}

ChatMessage &ensureToolDraft(
    ChatThread &chat,
    std::string const &toolCallId,
    std::string const &toolName
) {
    for (ChatMessage &message : chat.streamDraftMessages) {
        if (message.role == ChatRole::Tool && message.toolCallId == toolCallId) {
            if (!toolName.empty()) {
                message.toolName = toolName;
            }
            return message;
        }
    }
    chat.streamDraftMessages.push_back(ChatMessage {
        .role = ChatRole::Tool,
        .toolCallId = toolCallId,
        .toolName = toolName,
    });
    return chat.streamDraftMessages.back();
}

bool eraseChatMessageByIndex(ChatThread &chat, std::size_t index) {
    if (index < chat.messages.size()) {
        chat.messages.erase(chat.messages.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }
    std::size_t const draftIndex = index - chat.messages.size();
    if (draftIndex < chat.streamDraftMessages.size()) {
        chat.streamDraftMessages.erase(
            chat.streamDraftMessages.begin() + static_cast<std::ptrdiff_t>(draftIndex)
        );
        return true;
    }
    return false;
}

void setChatModel(AppState &state, int chatIndex, std::string path, std::string name) {
    if (chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= state.chats.size()) {
        return;
    }
    ChatThread &chat = state.chats[static_cast<std::size_t>(chatIndex)];
    chat.modelPath = std::move(path);
    chat.modelName = std::move(name);
}

std::vector<int> clearChatModelReferences(AppState &state, std::string const &modelPath) {
    std::vector<int> changed;
    if (modelPath.empty()) {
        return changed;
    }
    for (std::size_t i = 0; i < state.chats.size(); ++i) {
        ChatThread &chat = state.chats[i];
        if (chat.modelPath != modelPath) {
            continue;
        }
        chat.modelPath.clear();
        chat.modelName.clear();
        chat.updatedAtUnixMs = currentUnixMillis();
        changed.push_back(static_cast<int>(i));
    }
    return changed;
}

bool chatHasActiveGeneration(ChatThread const &chat) {
    return chat.streaming || chat.activeGenerationId != 0;
}

bool hasActiveGenerations(AppState const &state) {
    return std::any_of(state.chats.begin(), state.chats.end(), [](ChatThread const &chat) {
        return chatHasActiveGeneration(chat);
    });
}

void queuePendingChatSend(AppState &state, ChatThread const &chat, std::string message) {
    if (chat.id.empty() || message.empty()) {
        return;
    }
    state.pendingChatSends.push_back(PendingChatSend {
        .chatId = chat.id,
        .message = std::move(message),
        .modelPath = chat.modelPath,
        .modelName = chat.modelName,
    });
}

void clearPendingChatSendsForChat(AppState &state, std::string const &chatId) {
    if (chatId.empty()) {
        return;
    }
    std::erase_if(state.pendingChatSends, [&](PendingChatSend const &pending) {
        return pending.chatId == chatId;
    });
}

void clearPendingChatSendsForModel(AppState &state, std::string const &modelPath) {
    if (modelPath.empty()) {
        return;
    }
    std::erase_if(state.pendingChatSends, [&](PendingChatSend const &pending) {
        return pending.modelPath == modelPath;
    });
}

void requestModelLoadNow(AppState &state, IModelManager &manager, std::string const &path, std::string const &name) {
    if (path.empty()) {
        return;
    }
    std::string const displayName = !name.empty() ? name : modelDisplayName(path);
    state.modelLoading = true;
    state.pendingModelPath = path;
    state.pendingModelName = displayName;
    state.loadDefaults.modelPath = path;
    state.statusText = displayName.empty() ? "Loading model..." : "Loading " + displayName;
    state.errorText.clear();
    if (state.deferredModelPath == path) {
        state.deferredModelPath.clear();
        state.deferredModelName.clear();
    }
    state.latestLoadModelRequestId = manager.loadModel(state.loadDefaults);
}

bool startChatGeneration(
    AppState &state,
    IStore &catalog,
    IChatEngine &engine,
    int chatIndex,
    std::string const &message
) {
    if (message.empty() || chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= state.chats.size()) {
        return false;
    }

    bool const fakeStreaming = lambda_studio_backend::debugFakeStreamEnabled();
    ChatThread &chat = state.chats[static_cast<std::size_t>(chatIndex)];
    if (chat.streaming) {
        return false;
    }
    if (!fakeStreaming && (chat.modelPath.empty() || chat.modelPath != state.loadedModelPath || state.modelLoading)) {
        return false;
    }

    if (fakeStreaming && chat.modelPath.empty()) {
        chat.modelPath = "__debug_fake_stream__";
        chat.modelName = "Debug Stream";
        state.loadedModelPath = chat.modelPath;
        state.loadedModelName = chat.modelName;
    }

    if (chat.messages.empty() || chat.title == "New chat") {
        chat.title = titleFromPrompt(message);
    }

    chat.streamDraftMessages.clear();
    chat.updatedAtUnixMs = currentUnixMillis();
    chat.messages.push_back(lambda::ChatMessage {
        .role = ChatRole::User,
        .text = message,
    });
    chat.streaming = true;
    state.errorText.clear();
    state.statusText = "Generating response...";

    std::uint64_t const generationId = generateGenerationId();
    chat.activeGenerationId = generationId;

    persistChatThread(state, catalog, chatIndex);

    engine.startChat(
        toGenerationRequest(chat),
        [](lambda_studio_backend::LlmUiEvent event) {
            if (!Application::hasInstance()) {
                return;
            }
            Application::instance().eventQueue().post(std::move(event));
        }
    );
    return true;
}

bool maybeDispatchDeferredWork(
    AppState &state,
    IStore &catalog,
    IChatEngine &engine,
    IModelManager &manager
) {
    for (std::size_t i = 0; i < state.pendingChatSends.size();) {
        PendingChatSend const pending = state.pendingChatSends[i];
        int const chatIndex = chatIndexById(state, pending.chatId);
        if (chatIndex < 0 || pending.message.empty()) {
            state.pendingChatSends.erase(state.pendingChatSends.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }

        ChatThread const &chat = state.chats[static_cast<std::size_t>(chatIndex)];
        if (chat.modelPath.empty() || chat.modelPath != pending.modelPath) {
            state.pendingChatSends.erase(state.pendingChatSends.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }

        if (!state.modelLoading && chat.modelPath == state.loadedModelPath && !chat.streaming) {
            state.pendingChatSends.erase(state.pendingChatSends.begin() + static_cast<std::ptrdiff_t>(i));
            return startChatGeneration(state, catalog, engine, chatIndex, pending.message);
        }
        ++i;
    }

    if (state.modelLoading || hasActiveGenerations(state)) {
        return false;
    }

    for (std::size_t i = 0; i < state.pendingChatSends.size();) {
        PendingChatSend const pending = state.pendingChatSends[i];
        int const chatIndex = chatIndexById(state, pending.chatId);
        if (chatIndex < 0 || pending.message.empty()) {
            state.pendingChatSends.erase(state.pendingChatSends.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }

        ChatThread const &chat = state.chats[static_cast<std::size_t>(chatIndex)];
        if (chat.modelPath.empty() || chat.modelPath != pending.modelPath) {
            state.pendingChatSends.erase(state.pendingChatSends.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }

        if (chat.modelPath == state.loadedModelPath) {
            state.pendingChatSends.erase(state.pendingChatSends.begin() + static_cast<std::ptrdiff_t>(i));
            return startChatGeneration(state, catalog, engine, chatIndex, pending.message);
        }

        requestModelLoadNow(
            state,
            manager,
            chat.modelPath,
            !chat.modelName.empty() ? chat.modelName : pending.modelName
        );
        return true;
    }

    if (!state.deferredModelPath.empty() && state.deferredModelPath != state.loadedModelPath) {
        std::string const deferredPath = state.deferredModelPath;
        std::string const deferredName = state.deferredModelName;
        state.deferredModelPath.clear();
        state.deferredModelName.clear();
        requestModelLoadNow(state, manager, deferredPath, deferredName);
        return true;
    }

    return false;
}

std::vector<lambda_studio_backend::ChatMessage> toBackendMessages(ChatThread const &chat) {
    std::vector<lambda_studio_backend::ChatMessage> result;
    result.reserve(chat.messages.size());
    for (lambda::ChatMessage const &message : chat.messages) {
        lambda_studio_backend::ChatMessage backendMessage {
            .role = toBackendRole(message.role),
            .text = message.text,
            .toolCallId = message.toolCallId,
            .toolName = message.toolName,
            .toolState = toBackendToolExecutionState(message.toolState),
        };
        backendMessage.toolCalls.reserve(message.toolCalls.size());
        for (ChatToolCall const &toolCall : message.toolCalls) {
            backendMessage.toolCalls.push_back(toBackendToolCall(toolCall));
        }
        result.push_back(std::move(backendMessage));
    }
    return result;
}

lambda_studio_backend::ChatGenerationRequest toGenerationRequest(ChatThread const &chat) {
    return lambda_studio_backend::ChatGenerationRequest {
        .chatId = chat.id,
        .generationId = chat.activeGenerationId,
        .messages = toBackendMessages(chat),
        .requestGenerationParams = chat.generationDefaults,
        .summaryText = chat.summaryText,
        .summaryMessageCount = chat.summaryMessageCount,
        .summaryUpdatedAtUnixMs = chat.summaryUpdatedAtUnixMs,
    };
}

void applyGenerationPatch(
    lambda_studio_backend::GenerationParams &target,
    lambda_studio_backend::GenerationParamsPatch const &patch
) {
    if (patch.seed.has_value()) {
        target.seed = *patch.seed;
    }
    if (patch.maxTokens.has_value()) {
        target.maxTokens = *patch.maxTokens;
    }
    if (patch.topK.has_value()) {
        target.topK = *patch.topK;
    }
    if (patch.topP.has_value()) {
        target.topP = *patch.topP;
    }
    if (patch.minP.has_value()) {
        target.minP = *patch.minP;
    }
    if (patch.temp.has_value()) {
        target.temp = *patch.temp;
    }
    if (patch.penaltyLastN.has_value()) {
        target.penaltyLastN = *patch.penaltyLastN;
    }
    if (patch.repeatPenalty.has_value()) {
        target.repeatPenalty = *patch.repeatPenalty;
    }
    if (patch.frequencyPenalty.has_value()) {
        target.frequencyPenalty = *patch.frequencyPenalty;
    }
    if (patch.presencePenalty.has_value()) {
        target.presencePenalty = *patch.presencePenalty;
    }
    if (patch.mirostat.has_value()) {
        target.mirostat = *patch.mirostat;
    }
    if (patch.mirostatTau.has_value()) {
        target.mirostatTau = *patch.mirostatTau;
    }
    if (patch.mirostatEta.has_value()) {
        target.mirostatEta = *patch.mirostatEta;
    }
    if (patch.ignoreEos.has_value()) {
        target.ignoreEos = *patch.ignoreEos;
    }
}

void applySessionPatch(
    lambda_studio_backend::SessionParams &target,
    lambda_studio_backend::SessionParamsPatch const &patch
) {
    if (patch.nCtx.has_value()) {
        target.nCtx = *patch.nCtx;
    }
    if (patch.nBatch.has_value()) {
        target.nBatch = *patch.nBatch;
    }
    if (patch.nUBatch.has_value()) {
        target.nUBatch = *patch.nUBatch;
    }
    if (patch.enableThinking.has_value()) {
        target.enableThinking = *patch.enableThinking;
    }
    if (patch.systemPrompt.has_value()) {
        target.systemPrompt = *patch.systemPrompt;
    }
    if (patch.chatTemplate.has_value()) {
        target.chatTemplate = *patch.chatTemplate;
    }
    if (patch.flashAttn.has_value()) {
        target.flashAttn = *patch.flashAttn;
    }
    if (patch.toolsEnabled.has_value()) {
        target.toolConfig.enabled = *patch.toolsEnabled;
    }
    if (patch.maxToolCalls.has_value()) {
        target.toolConfig.maxToolCalls = *patch.maxToolCalls;
    }
    if (patch.toolWorkspaceRoot.has_value()) {
        target.toolConfig.workspaceRoot = lambda_studio_backend::normalizeToolWorkspaceRoot(*patch.toolWorkspaceRoot);
    }
    if (patch.enabledToolNames.has_value()) {
        target.toolConfig.enabledToolNames = *patch.enabledToolNames;
    }
}

void applyLoadPatch(
    lambda_studio_backend::LoadParams &target,
    lambda_studio_backend::LoadParamsPatch const &patch
) {
    if (patch.modelPath.has_value()) {
        target.modelPath = *patch.modelPath;
    }
    if (patch.nGpuLayers.has_value()) {
        target.nGpuLayers = *patch.nGpuLayers;
    }
    if (patch.nCtx.has_value()) {
        target.nCtx = *patch.nCtx;
    }
    if (patch.nBatch.has_value()) {
        target.nBatch = *patch.nBatch;
    }
    if (patch.nUBatch.has_value()) {
        target.nUBatch = *patch.nUBatch;
    }
    if (patch.useMmap.has_value()) {
        target.useMmap = *patch.useMmap;
    }
    if (patch.useMlock.has_value()) {
        target.useMlock = *patch.useMlock;
    }
    if (patch.embeddings.has_value()) {
        target.embeddings = *patch.embeddings;
    }
    if (patch.offloadKqv.has_value()) {
        target.offloadKqv = *patch.offloadKqv;
    }
    if (patch.flashAttn.has_value()) {
        target.flashAttn = *patch.flashAttn;
    }
}

lambda_studio_backend::GenerationParamsPatch patchFromGenerationParams(
    lambda_studio_backend::GenerationParams const &params
) {
    return lambda_studio_backend::GenerationParamsPatch {
        .seed = params.seed,
        .maxTokens = params.maxTokens,
        .topK = params.topK,
        .topP = params.topP,
        .minP = params.minP,
        .temp = params.temp,
        .penaltyLastN = params.penaltyLastN,
        .repeatPenalty = params.repeatPenalty,
        .frequencyPenalty = params.frequencyPenalty,
        .presencePenalty = params.presencePenalty,
        .mirostat = params.mirostat,
        .mirostatTau = params.mirostatTau,
        .mirostatEta = params.mirostatEta,
        .ignoreEos = params.ignoreEos,
    };
}

lambda_studio_backend::LoadParamsPatch patchFromLoadParams(lambda_studio_backend::LoadParams const &params) {
    return lambda_studio_backend::LoadParamsPatch {
        .modelPath = params.modelPath,
        .nGpuLayers = params.nGpuLayers,
        .nCtx = params.nCtx,
        .nBatch = params.nBatch,
        .nUBatch = params.nUBatch,
        .useMmap = params.useMmap,
        .useMlock = params.useMlock,
        .embeddings = params.embeddings,
        .offloadKqv = params.offloadKqv,
        .flashAttn = params.flashAttn,
    };
}

lambda_studio_backend::SessionParamsPatch patchFromSessionParams(lambda_studio_backend::SessionParams const &params) {
    return lambda_studio_backend::SessionParamsPatch {
        .nCtx = params.nCtx,
        .nBatch = params.nBatch,
        .nUBatch = params.nUBatch,
        .enableThinking = params.enableThinking,
        .systemPrompt = params.systemPrompt,
        .chatTemplate = params.chatTemplate,
        .flashAttn = params.flashAttn,
        .toolsEnabled = params.toolConfig.enabled,
        .maxToolCalls = params.toolConfig.maxToolCalls,
        .toolWorkspaceRoot = params.toolConfig.workspaceRoot,
        .enabledToolNames = params.toolConfig.enabledToolNames,
    };
}

std::string applyScopeLabel(lambda_studio_backend::ApplyScope scope) {
    switch (scope) {
    case lambda_studio_backend::ApplyScope::AppliedImmediately:
        return "Applies now";
    case lambda_studio_backend::ApplyScope::Deferred:
        return "Deferred";
    case lambda_studio_backend::ApplyScope::RequiresSessionReset:
        return "Requires session reset";
    case lambda_studio_backend::ApplyScope::RequiresModelReload:
        return "Requires reload";
    case lambda_studio_backend::ApplyScope::Rejected:
        return "Rejected";
    }
    return "Applies now";
}

std::string titleFromPrompt(std::string const &prompt) {
    std::string title = prompt;
    while (!title.empty() && std::isspace(static_cast<unsigned char>(title.front()))) {
        title.erase(title.begin());
    }
    if (title.empty()) {
        return "New chat";
    }
    if (title.size() > 40) {
        title.resize(40);
        title += "...";
    }
    return title;
}

} // namespace

struct StudioApp : ViewModifiers<StudioApp> {
    std::shared_ptr<AppRuntime> runtime;

    Element body() const {
        auto theme = useEnvironmentReactive<ThemeKey>();
        auto appState = useState<AppState>(makeInitialAppState());
        std::shared_ptr<AppRuntime> runtimeInstance = runtime;
        if (!runtimeInstance) {
            return Element {
                Text {
                    .text = "Runtime configuration missing",
                    .font = Font::body(),
                    .color = Color::danger(),
                    .horizontalAlignment = HorizontalAlignment::Center,
                    .verticalAlignment = VerticalAlignment::Center,
                }
            };
        }
        std::shared_ptr<IChatEngine> engine = runtimeInstance->engine;
        std::shared_ptr<IModelManager> manager = runtimeInstance->manager;
        std::shared_ptr<IStore> catalog = runtimeInstance->catalog;
        auto currentRemoteSearchKey = [](AppState const &state) {
            return remoteModelSearchCacheKey(
                state.modelSearchQuery,
                state.remoteModelSort,
                state.remoteModelVisibility
            );
        };

        auto handlersRegistered = useState(false);
        if (!(*handlersRegistered)) {
            handlersRegistered = true;
            Application::instance().eventQueue().on<lambda_studio_backend::LlmUiEvent>(
                [appState, catalog, engine, manager](lambda_studio_backend::LlmUiEvent const &event) {
                    lambda_studio_backend::debugToolTrace(
                        string_format(
                            "ui.LlmUiEvent kind=%d chat_id=%s generation_id=%llu tool_call_id=%s tool_name=%s tool_state=%d text=%s",
                            static_cast<int>(event.kind),
                            event.chatId.c_str(),
                            static_cast<unsigned long long>(event.generationId),
                            event.toolCallId.c_str(),
                            event.toolName.c_str(),
                            static_cast<int>(event.toolState),
                            event.text.substr(0, 256).c_str()
                        )
                    );
                    AppState nextState = *appState;
                    auto it = std::find_if(nextState.chats.begin(), nextState.chats.end(), [&](ChatThread const &chat) {
                        return chat.id == event.chatId;
                    });
                    if (it == nextState.chats.end()) {
                        return;
                    }
                    if (it->activeGenerationId == 0 || event.generationId == 0 ||
                        event.generationId != it->activeGenerationId) {
                        return;
                    }

                    int const chatIndex = static_cast<int>(std::distance(nextState.chats.begin(), it));

                    std::int64_t const nowNanos = steadyNowNanos();
                    bool persistAfterEvent = true;

                    if (event.kind == lambda_studio_backend::LlmUiEvent::Kind::Chunk) {
                        persistAfterEvent = false;
                        ChatRole const role = event.part == lambda_studio_backend::LlmUiEvent::Part::Thinking
                                                  ? ChatRole::Reasoning
                                                  : ChatRole::Assistant;
                        if (role != ChatRole::Reasoning) {
                            finishTrailingReasoningMessage(it->streamDraftMessages, nowNanos);
                        }
                        if (it->streamDraftMessages.empty() || it->streamDraftMessages.back().role != role) {
                            it->streamDraftMessages.push_back(lambda::ChatMessage {
                                .role = role,
                                .text = "",
                                .startedAtNanos = role == ChatRole::Reasoning ? nowNanos : 0,
                                .collapsed = role == ChatRole::Reasoning,
                            });
                        }
                        appendChatMessageText(it->streamDraftMessages.back(), event.text);
                    } else if (event.kind == lambda_studio_backend::LlmUiEvent::Kind::ToolCallsParsed) {
                        finishTrailingReasoningMessage(it->streamDraftMessages, nowNanos);
                        ChatMessage &assistant = ensureAssistantDraftForToolCalls(*it);
                        assistant.toolCalls.clear();
                        assistant.toolCalls.reserve(event.toolCalls.size());
                        for (lambda_studio_backend::ToolCall const &toolCall : event.toolCalls) {
                            assistant.toolCalls.push_back(toChatToolCall(toolCall));
                        }
                        if (event.generationStats.has_value()) {
                            assistant.generationStats = toMessageGenerationStats(*event.generationStats, *it);
                        }
                        it->updatedAtUnixMs = currentUnixMillis();
                        nextState.statusText = event.text.empty() ? "Assistant requested tools" :
                                                                    "Assistant requested tools (" + event.text + ")";
                    } else if (event.kind == lambda_studio_backend::LlmUiEvent::Kind::ToolApprovalRequested) {
                        finishTrailingReasoningMessage(it->streamDraftMessages, nowNanos);
                        ChatMessage &tool = ensureToolDraft(*it, event.toolCallId, event.toolName);
                        tool.toolState = toToolMessageState(event.toolState);
                        tool.toolName = event.toolName;
                        tool.toolCallId = event.toolCallId;
                        tool.collapsed = false;
                        if (tool.text.empty() && !event.toolCall.arguments.empty()) {
                            tool.text = event.toolCall.arguments;
                            ++tool.textRevision;
                        }
                        it->updatedAtUnixMs = currentUnixMillis();
                        nextState.statusText = event.toolName.empty() ? "Tool approval required" :
                                                                        "Approval required for " + event.toolName;
                    } else if (event.kind == lambda_studio_backend::LlmUiEvent::Kind::ToolStarted) {
                        finishTrailingReasoningMessage(it->streamDraftMessages, nowNanos);
                        ChatMessage &tool = ensureToolDraft(*it, event.toolCallId, event.toolName);
                        tool.toolState = toToolMessageState(event.toolState);
                        tool.toolName = event.toolName;
                        tool.toolCallId = event.toolCallId;
                        tool.collapsed = false;
                        if (tool.text.empty() && !event.toolCall.arguments.empty()) {
                            tool.text = event.toolCall.arguments;
                            ++tool.textRevision;
                        }
                        it->updatedAtUnixMs = currentUnixMillis();
                        nextState.statusText = event.toolName.empty() ? "Running tool..." :
                                                                        "Running " + event.toolName;
                    } else if (event.kind == lambda_studio_backend::LlmUiEvent::Kind::ToolFinished ||
                               event.kind == lambda_studio_backend::LlmUiEvent::Kind::ToolDenied ||
                               event.kind == lambda_studio_backend::LlmUiEvent::Kind::ToolFailed) {
                        ChatMessage &tool = ensureToolDraft(*it, event.toolCallId, event.toolName);
                        tool.toolState = toToolMessageState(event.toolState);
                        tool.toolName = event.toolName;
                        tool.toolCallId = event.toolCallId;
                        tool.text = event.text;
                        ++tool.textRevision;
                        tool.collapsed = true;
                        it->updatedAtUnixMs = currentUnixMillis();
                        if (event.kind == lambda_studio_backend::LlmUiEvent::Kind::ToolFinished) {
                            nextState.statusText = event.toolName.empty() ? "Tool finished" :
                                                                            event.toolName + " finished";
                        } else if (event.kind == lambda_studio_backend::LlmUiEvent::Kind::ToolDenied) {
                            nextState.statusText = event.toolName.empty() ? "Tool denied" :
                                                                            event.toolName + " denied";
                        } else {
                            nextState.statusText = event.toolName.empty() ? "Tool failed" :
                                                                            event.toolName + " failed";
                        }
                    } else if (event.kind == lambda_studio_backend::LlmUiEvent::Kind::Done) {
                        if (event.summaryText != it->summaryText ||
                            event.summaryMessageCount != it->summaryMessageCount ||
                            event.summaryUpdatedAtUnixMs != it->summaryUpdatedAtUnixMs) {
                            it->summaryText = event.summaryText;
                            it->summaryMessageCount = event.summaryMessageCount;
                            it->summaryUpdatedAtUnixMs = event.summaryUpdatedAtUnixMs;
                        }
                        commitStreamDraft(*it, nowNanos);
                        it->streaming = false;
                        it->activeGenerationId = 0;
                        it->updatedAtUnixMs = currentUnixMillis();
                        if (event.generationStats.has_value()) {
                            attachGenerationStatsToLatestResponseMessages(*it, *event.generationStats);
                        }
                        nextState.statusText = "Response complete";
                    } else if (event.kind == lambda_studio_backend::LlmUiEvent::Kind::Error) {
                        if (event.summaryText != it->summaryText ||
                            event.summaryMessageCount != it->summaryMessageCount ||
                            event.summaryUpdatedAtUnixMs != it->summaryUpdatedAtUnixMs) {
                            it->summaryText = event.summaryText;
                            it->summaryMessageCount = event.summaryMessageCount;
                            it->summaryUpdatedAtUnixMs = event.summaryUpdatedAtUnixMs;
                        }
                        commitStreamDraft(*it, nowNanos);
                        it->streaming = false;
                        it->activeGenerationId = 0;
                        it->messages.push_back(lambda::ChatMessage {
                            .role = ChatRole::Assistant,
                            .text = std::string("[Error] ") + event.text,
                        });
                        if (event.generationStats.has_value()) {
                            attachGenerationStatsToLatestResponseMessages(*it, *event.generationStats);
                        }
                        syncAssistantParagraphs(it->messages.back());
                        it->updatedAtUnixMs = currentUnixMillis();
                        nextState.errorText = event.text;
                    }

                    if (persistAfterEvent) {
                        persistChatThread(nextState, *catalog, chatIndex);
                    }
                    if (event.kind != lambda_studio_backend::LlmUiEvent::Kind::Chunk) {
                        maybeDispatchDeferredWork(nextState, *catalog, *engine, *manager);
                    }
                    appState = std::move(nextState);
                }
            );

            Application::instance().eventQueue().on<lambda_studio_backend::ModelManagerEvent>(
                [appState, manager, catalog, engine, currentRemoteSearchKey](lambda_studio_backend::ModelManagerEvent const &event) {
                    AppState nextState = *appState;
                    auto isStaleLatest = [](std::uint64_t eventId, std::uint64_t latestId) {
                        return latestId > 0 && eventId > 0 && eventId < latestId;
                    };
                    switch (event.kind) {
                    case lambda_studio_backend::ModelManagerEvent::Kind::LocalModelsReady:
                        if (isStaleLatest(event.requestId, nextState.latestInventoryRequestId)) {
                            break;
                        }
                        nextState.refreshingModels = false;
                        nextState.localModels.clear();
                        nextState.localModels.reserve(event.localModels.size());
                        for (lambda_studio_backend::LocalModelInfo const &model : event.localModels) {
                            if (!model.path.empty()) {
                                nextState.localModels.push_back(toLocalModel(model));
                            }
                        }
                        try {
                            catalog->replaceLocalModelInstances(nextState.localModels);
                        } catch (std::exception const &e) {
                            nextState.errorText = e.what();
                        }
                        nextState.statusText = nextState.localModels.empty()
                                                   ? "No local models found"
                                                   : "Found " + std::to_string(nextState.localModels.size()) + " local model" +
                                                         (nextState.localModels.size() == 1 ? "" : "s");
                        break;
                    case lambda_studio_backend::ModelManagerEvent::Kind::ModelLoaded:
                        if (isStaleLatest(event.requestId, nextState.latestLoadModelRequestId)) {
                            break;
                        }
                        nextState.modelLoading = false;
                        nextState.loadDefaults = event.appliedLoadParams;
                        nextState.loadedModelPath = event.modelPath;
                        nextState.loadedModelName = event.modelName;
                        nextState.pendingModelPath.clear();
                        nextState.pendingModelName.clear();
                        nextState.statusText = "Loaded " + event.modelName;
                        nextState.errorText.clear();
                        break;
                    case lambda_studio_backend::ModelManagerEvent::Kind::ModelLoadError:
                        if (isStaleLatest(event.requestId, nextState.latestLoadModelRequestId)) {
                            break;
                        }
                        nextState.modelLoading = false;
                        nextState.pendingModelPath.clear();
                        nextState.pendingModelName.clear();
                        clearPendingChatSendsForModel(nextState, event.appliedLoadParams.modelPath);
                        nextState.errorText = event.error;
                        break;
                    case lambda_studio_backend::ModelManagerEvent::Kind::ModelDeleted:
                    {
                        if (isStaleLatest(event.requestId, nextState.latestDeleteModelRequestId)) {
                            break;
                        }
                        nextState.modelDeleting = false;
                        nextState.pendingDeleteModelPath.clear();
                        if (event.modelPath == nextState.loadedModelPath) {
                            nextState.loadedModelPath.clear();
                            nextState.loadedModelName.clear();
                            nextState.modelLoading = false;
                            if (event.modelPath == nextState.pendingModelPath) {
                                nextState.pendingModelPath.clear();
                                nextState.pendingModelName.clear();
                            }
                        }
                        {
                            std::vector<int> const affectedChats = clearChatModelReferences(nextState, event.modelPath);
                            for (int chatIndex : affectedChats) {
                                persistChatThread(nextState, *catalog, chatIndex);
                            }
                        }
                        clearPendingChatSendsForModel(nextState, event.modelPath);
                        if (nextState.deferredModelPath == event.modelPath) {
                            nextState.deferredModelPath.clear();
                            nextState.deferredModelName.clear();
                        }
                        std::string const deletedName =
                            !event.modelName.empty() ? event.modelName : modelDisplayName(event.modelPath);
                        nextState.statusText = deletedName.empty() ? "Deleted model" : "Deleted " + deletedName;
                        nextState.errorText.clear();
                        break;
                    }
                    case lambda_studio_backend::ModelManagerEvent::Kind::ModelDeleteError:
                        if (isStaleLatest(event.requestId, nextState.latestDeleteModelRequestId)) {
                            break;
                        }
                        nextState.modelDeleting = false;
                        nextState.pendingDeleteModelPath.clear();
                        nextState.errorText = event.error;
                        break;
                    case lambda_studio_backend::ModelManagerEvent::Kind::DownloadProgress:
                        if (isStaleLatest(event.requestId, nextState.latestDownloadRequestId)) {
                            break;
                        }
                        if (!nextState.pendingDownloadJobId.empty()) {
                            for (DownloadJob &job : nextState.recentDownloadJobs) {
                                if (job.id != nextState.pendingDownloadJobId) {
                                    continue;
                                }
                                job.downloadedBytes = event.downloadedBytes;
                                job.totalBytes = event.totalBytes;
                                break;
                            }
                        }
                        break;
                    case lambda_studio_backend::ModelManagerEvent::Kind::DownloadDone:
                        if (isStaleLatest(event.requestId, nextState.latestDownloadRequestId)) {
                            break;
                        }
                        nextState.downloadingModel = false;
                        if (!nextState.pendingDownloadJobId.empty()) {
                            for (DownloadJob &job : nextState.recentDownloadJobs) {
                                if (job.id != nextState.pendingDownloadJobId) {
                                    continue;
                                }
                                job.status = DownloadJobStatus::Completed;
                                job.localPath = event.modelPath;
                                job.error.clear();
                                job.finishedAtUnixMs = currentUnixMillis();
                                job.downloadedBytes = event.totalBytes;
                                job.totalBytes = event.totalBytes;
                                try {
                                    catalog->finishDownloadJob(job.id, job.localPath, job.finishedAtUnixMs);
                                } catch (std::exception const &e) {
                                    nextState.errorText = e.what();
                                }
                                break;
                            }
                        }
                        nextState.pendingDownloadJobId.clear();
                        nextState.pendingDownloadRepoId.clear();
                        nextState.pendingDownloadFilePath.clear();
                        nextState.statusText = "Downloaded " + event.modelName;
                        nextState.errorText.clear();
                        break;
                    case lambda_studio_backend::ModelManagerEvent::Kind::DownloadError:
                        if (isStaleLatest(event.requestId, nextState.latestDownloadRequestId)) {
                            break;
                        }
                        nextState.downloadingModel = false;
                        if (!nextState.pendingDownloadJobId.empty()) {
                            for (DownloadJob &job : nextState.recentDownloadJobs) {
                                if (job.id != nextState.pendingDownloadJobId) {
                                    continue;
                                }
                                job.status = DownloadJobStatus::Failed;
                                job.error = event.error;
                                job.finishedAtUnixMs = currentUnixMillis();
                                if (event.downloadedBytes > 0) {
                                    job.downloadedBytes = event.downloadedBytes;
                                }
                                if (event.totalBytes > 0) {
                                    job.totalBytes = event.totalBytes;
                                }
                                try {
                                    catalog->failDownloadJob(job.id, job.error, job.finishedAtUnixMs);
                                } catch (std::exception const &e) {
                                    nextState.errorText = e.what();
                                }
                                break;
                            }
                        }
                        nextState.pendingDownloadJobId.clear();
                        nextState.pendingDownloadRepoId.clear();
                        nextState.pendingDownloadFilePath.clear();
                        nextState.errorText = event.error;
                        break;
                    case lambda_studio_backend::ModelManagerEvent::Kind::HfSearchReady:
                        if (isStaleLatest(event.requestId, nextState.latestSearchRequestId)) {
                            break;
                        }
                        nextState.searchingRemoteModels = false;
                        if (event.searchKey != currentRemoteSearchKey(nextState)) {
                            if (event.error.empty()) {
                                std::vector<RemoteModel> cachedModels;
                                cachedModels.reserve(event.hfModels.size());
                                for (lambda_studio_backend::HfModelInfo const &model : event.hfModels) {
                                    cachedModels.push_back(toRemoteModel(model));
                                }
                                try {
                                    catalog->replaceSearchSnapshot(
                                        event.searchKey,
                                        cachedModels,
                                        event.rawJson
                                    );
                                } catch (...) {
                                }
                            }
                            break;
                        }
                        if (!event.error.empty()) {
                            nextState.errorText = event.error;
                            if (nextState.remoteModels.empty()) {
                                nextState.selectedRemoteRepoId.clear();
                                nextState.selectedRemoteRepoFiles.clear();
                                nextState.selectedRemoteRepoDetail.reset();
                                nextState.loadingRemoteModelFiles = false;
                                nextState.loadingRemoteRepoDetail = false;
                            } else {
                                nextState.statusText = "Search refresh failed, showing cached results";
                            }
                        } else {
                            nextState.errorText.clear();
                            nextState.remoteModels.clear();
                            nextState.remoteModels.reserve(event.hfModels.size());
                            for (lambda_studio_backend::HfModelInfo const &model : event.hfModels) {
                                nextState.remoteModels.push_back(toRemoteModel(model));
                            }
                            try {
                                catalog->replaceSearchSnapshot(
                                    event.searchKey,
                                    nextState.remoteModels,
                                    event.rawJson
                                );
                            } catch (std::exception const &e) {
                                nextState.errorText = e.what();
                            }

                            if (nextState.remoteModels.empty()) {
                                nextState.selectedRemoteRepoId.clear();
                                nextState.selectedRemoteRepoFiles.clear();
                                nextState.selectedRemoteRepoDetail.reset();
                                nextState.loadingRemoteModelFiles = false;
                                nextState.loadingRemoteRepoDetail = false;
                                nextState.statusText = "No matching Hugging Face models";
                            } else {
                                bool foundSelection = false;
                                for (RemoteModel const &model : nextState.remoteModels) {
                                    if (model.id == nextState.selectedRemoteRepoId) {
                                        foundSelection = true;
                                        break;
                                    }
                                }
                                if (!foundSelection) {
                                    nextState.selectedRemoteRepoId = nextState.remoteModels.front().id;
                                    nextState.selectedRemoteRepoFiles.clear();
                                    nextState.selectedRemoteRepoDetail.reset();
                                    nextState.loadingRemoteModelFiles = true;
                                    nextState.loadingRemoteRepoDetail = true;
                                    nextState.latestRepoInspectRequestId = manager->inspectRepo(nextState.selectedRemoteRepoId);
                                }
                                nextState.statusText =
                                    "Found " + std::to_string(nextState.remoteModels.size()) + " matching model" +
                                    (nextState.remoteModels.size() == 1 ? "" : "s");
                            }
                        }
                        break;
                    case lambda_studio_backend::ModelManagerEvent::Kind::HfFilesReady:
                        if (isStaleLatest(event.requestId, nextState.latestRepoInspectRequestId)) {
                            break;
                        }
                        if (event.repoId == nextState.selectedRemoteRepoId) {
                            nextState.loadingRemoteModelFiles = false;
                        }
                        if (!event.error.empty()) {
                            nextState.errorText = event.error;
                            if (event.repoId == nextState.selectedRemoteRepoId && !nextState.selectedRemoteRepoFiles.empty()) {
                                nextState.statusText = "File refresh failed, showing cached repo files";
                            }
                        } else {
                            std::vector<RemoteModelFile> files;
                            files.reserve(event.hfFiles.size());
                            for (lambda_studio_backend::HfFileInfo const &file : event.hfFiles) {
                                files.push_back(toRemoteModelFile(file));
                            }
                            try {
                                catalog->replaceRepoFilesSnapshot(
                                    event.repoId,
                                    files,
                                    event.rawJson
                                );
                            } catch (std::exception const &e) {
                                nextState.errorText = e.what();
                            }
                            if (event.repoId == nextState.selectedRemoteRepoId) {
                                nextState.errorText.clear();
                                nextState.selectedRemoteRepoFiles = std::move(files);
                                nextState.statusText = event.hfFiles.empty()
                                                           ? "No GGUF files in selected repo"
                                                           : "Found " + std::to_string(event.hfFiles.size()) + " GGUF file" +
                                                                 (event.hfFiles.size() == 1 ? "" : "s");
                            }
                        }
                        break;
                    case lambda_studio_backend::ModelManagerEvent::Kind::HfRepoDetailReady:
                        if (isStaleLatest(event.requestId, nextState.latestRepoInspectRequestId)) {
                            break;
                        }
                        if (event.repoId == nextState.selectedRemoteRepoId) {
                            nextState.loadingRemoteRepoDetail = false;
                        }
                        if (!event.error.empty()) {
                            nextState.errorText = event.error;
                            if (event.repoId == nextState.selectedRemoteRepoId &&
                                nextState.selectedRemoteRepoDetail.has_value()) {
                                nextState.statusText = "Repo detail refresh failed, showing cached metadata";
                            }
                        } else {
                            RemoteRepoDetail detail = toRemoteRepoDetail(event.hfRepoDetail);
                            try {
                                catalog->replaceRepoDetailSnapshot(detail, event.rawJson);
                            } catch (std::exception const &e) {
                                nextState.errorText = e.what();
                            }
                            if (event.repoId == nextState.selectedRemoteRepoId) {
                                nextState.errorText.clear();
                                nextState.selectedRemoteRepoDetail = std::move(detail);
                            }
                        }
                        break;
                    }
                    appState = std::move(nextState);
                    if (event.kind == lambda_studio_backend::ModelManagerEvent::Kind::ModelLoaded ||
                        event.kind == lambda_studio_backend::ModelManagerEvent::Kind::ModelLoadError ||
                        event.kind == lambda_studio_backend::ModelManagerEvent::Kind::ModelDeleted) {
                        AppState postState = *appState;
                        if (maybeDispatchDeferredWork(postState, *catalog, *engine, *manager)) {
                            appState = std::move(postState);
                        }
                    }
                }
            );

            AppState nextState = *appState;
            nextState.refreshingModels = true;
            try {
                PersistedChatState persistedChatState = catalog->loadPersistedChatState();
                nextState.chats = std::move(persistedChatState.chats);
                for (ChatThread &chat : nextState.chats) {
                    expirePendingToolApprovals(chat);
                }
                restoreSelectedChat(nextState, persistedChatState.selectedChatId);
                catalog->markRunningDownloadJobsInterrupted(currentUnixMillis());
                nextState.recentDownloadJobs = catalog->loadRecentDownloadJobs();
                nextState.localModels = catalog->loadLocalModelInstances();
                if (auto persistedDefaults = catalog->loadEngineConfigDefaults(); persistedDefaults.has_value()) {
                    nextState.loadDefaults = persistedDefaults->loadDefaults;
                    nextState.sessionDefaults = persistedDefaults->sessionDefaults;
                    nextState.generationDefaults = persistedDefaults->generationDefaults;
                    if (!nextState.loadDefaults.modelPath.empty()) {
                        engine->updateLoadParams(patchFromLoadParams(nextState.loadDefaults));
                    }
                    engine->updateSessionDefaults(patchFromSessionParams(nextState.sessionDefaults));
                    engine->updateGenerationDefaults(patchFromGenerationParams(nextState.generationDefaults));
                }
            } catch (std::exception const &e) {
                nextState.errorText = e.what();
            }
            nextState.loadDefaults = engine->loadParams();
            if (nextState.loadDefaults.modelPath.empty()) {
                nextState.loadDefaults = lambda_studio_backend::defaultLoadParams();
            }
            nextState.generationDefaults = engine->generationDefaults();
            nextState.sessionDefaults = engine->sessionDefaults();
            nextState.loadedModelPath = nextState.loadDefaults.modelPath;
            nextState.loadedModelName = lambda_studio_backend::defaultModelName();
            for (ChatThread const &chat : nextState.chats) {
                if (!chat.generationDefaults.has_value()) {
                    continue;
                }
                engine->updateChatGenerationParams(chat.id, patchFromGenerationParams(*chat.generationDefaults));
            }
            if (!nextState.loadedModelPath.empty() && !engine->isLoaded()) {
                nextState.modelLoading = true;
                nextState.pendingModelPath = nextState.loadedModelPath;
                nextState.pendingModelName = nextState.loadedModelName;
            }
            nextState.latestInventoryRequestId = manager->refreshLocalModels();
            if (!lambda_studio_backend::defaultModelPath().empty() && !engine->isLoaded()) {
                lambda_studio_backend::LoadParams initialLoad = nextState.loadDefaults;
                initialLoad.modelPath = lambda_studio_backend::defaultModelPath();
                nextState.latestLoadModelRequestId = manager->loadModel(std::move(initialLoad));
            }
            appState = std::move(nextState);
        }

        AppState const &state = *appState;

        auto requestInventoryRefresh = [appState, manager]() {
            AppState nextState = *appState;
            nextState.refreshingModels = true;
            nextState.statusText = "Refreshing model inventory...";
            nextState.errorText.clear();
            nextState.latestInventoryRequestId = manager->refreshLocalModels();
            appState = std::move(nextState);
        };

        auto updateModelSearchQuery = [appState](std::string const &query) {
            AppState nextState = *appState;
            nextState.modelSearchQuery = query;
            appState = std::move(nextState);
        };

        auto updateRemoteModelSort = [appState](RemoteModelSort sort) {
            AppState nextState = *appState;
            nextState.remoteModelSort = sort;
            appState = std::move(nextState);
        };

        auto updateRemoteModelVisibility = [appState](RemoteModelVisibilityFilter visibility) {
            AppState nextState = *appState;
            nextState.remoteModelVisibility = visibility;
            appState = std::move(nextState);
        };

        auto requestRemoteSearch = [appState, catalog, manager, currentRemoteSearchKey](
                                       std::string query,
                                       RemoteModelSort sort,
                                       RemoteModelVisibilityFilter visibility
                                   ) {
            AppState nextState = *appState;
            nextState.modelSearchQuery = query;
            nextState.remoteModelSort = sort;
            nextState.remoteModelVisibility = visibility;
            nextState.searchingRemoteModels = true;
            std::string const cacheKey = currentRemoteSearchKey(nextState);
            try {
                nextState.remoteModels = catalog->loadSearchResults(cacheKey);
                if (nextState.remoteModels.empty()) {
                    nextState.remoteModels = catalog->searchCatalogModels(query, sort, visibility);
                }
                nextState.selectedRemoteRepoId.clear();
                nextState.selectedRemoteRepoFiles.clear();
                nextState.selectedRemoteRepoDetail.reset();
                nextState.loadingRemoteModelFiles = false;
                nextState.loadingRemoteRepoDetail = false;
                if (!nextState.remoteModels.empty()) {
                    nextState.selectedRemoteRepoId = nextState.remoteModels.front().id;
                    nextState.selectedRemoteRepoFiles = catalog->loadRepoFiles(nextState.selectedRemoteRepoId);
                    nextState.selectedRemoteRepoDetail = catalog->loadRepoDetail(nextState.selectedRemoteRepoId);
                    nextState.loadingRemoteModelFiles = nextState.selectedRemoteRepoFiles.empty();
                    nextState.loadingRemoteRepoDetail = !nextState.selectedRemoteRepoDetail.has_value();
                }
            } catch (std::exception const &e) {
                nextState.remoteModels.clear();
                nextState.selectedRemoteRepoId.clear();
                nextState.selectedRemoteRepoFiles.clear();
                nextState.selectedRemoteRepoDetail.reset();
                nextState.loadingRemoteModelFiles = false;
                nextState.loadingRemoteRepoDetail = false;
                nextState.errorText = e.what();
            }
            nextState.statusText = query.empty() ? "Searching top GGUF repositories..." :
                                                                  "Searching Hugging Face...";
            nextState.latestSearchRequestId = manager->searchHuggingFace(lambda_studio_backend::HfSearchRequest {
                .query = std::move(query),
                .sortKey = sort == RemoteModelSort::Likes ? "likes" :
                           sort == RemoteModelSort::Updated ? "lastModified" :
                                                              "downloads",
                .visibilityFilter = visibility == RemoteModelVisibilityFilter::PublicOnly ? "public" :
                                    visibility == RemoteModelVisibilityFilter::GatedOnly ? "gated" :
                                                                                           "all",
                .cacheKey = cacheKey,
            });
            appState = std::move(nextState);
        };

        auto requestRemoteRepoFiles = [appState, catalog, manager](std::string repoId) {
            if (repoId.empty()) {
                return;
            }
            AppState nextState = *appState;
            nextState.selectedRemoteRepoId = repoId;
            try {
                nextState.selectedRemoteRepoFiles = catalog->loadRepoFiles(repoId);
                nextState.selectedRemoteRepoDetail = catalog->loadRepoDetail(repoId);
                nextState.loadingRemoteModelFiles = nextState.selectedRemoteRepoFiles.empty();
                nextState.loadingRemoteRepoDetail = !nextState.selectedRemoteRepoDetail.has_value();
                nextState.errorText.clear();
            } catch (std::exception const &e) {
                nextState.selectedRemoteRepoFiles.clear();
                nextState.selectedRemoteRepoDetail.reset();
                nextState.loadingRemoteModelFiles = true;
                nextState.loadingRemoteRepoDetail = true;
                nextState.errorText = e.what();
            }
            nextState.statusText = "Loading GGUF files...";
            nextState.latestRepoInspectRequestId = manager->inspectRepo(std::move(repoId));
            appState = std::move(nextState);
        };

        auto requestRemoteDownload = [appState, catalog, manager](std::string repoId, std::string path) {
            if (repoId.empty() || path.empty()) {
                return;
            }
            AppState nextState = *appState;
            removeDownloadJobsForArtifact(nextState, repoId, path);
            DownloadJob job;
            job.id = repoId + "|" + path + "|" + std::to_string(currentUnixMillis());
            job.repoId = repoId;
            job.filePath = path;
            job.startedAtUnixMs = currentUnixMillis();
            job.status = DownloadJobStatus::Running;
            job.downloadedBytes = 0;
            job.totalBytes = 0;
            nextState.recentDownloadJobs.insert(nextState.recentDownloadJobs.begin(), job);
            if (nextState.recentDownloadJobs.size() > 12) {
                nextState.recentDownloadJobs.resize(12);
            }
            nextState.downloadingModel = true;
            nextState.pendingDownloadJobId = job.id;
            nextState.pendingDownloadRepoId = repoId;
            nextState.pendingDownloadFilePath = path;
            nextState.statusText = "Downloading " + path;
            nextState.errorText.clear();
            try {
                catalog->deleteDownloadJobsForArtifact(repoId, path);
                catalog->startDownloadJob(job);
            } catch (std::exception const &e) {
                nextState.errorText = e.what();
            }
            nextState.latestDownloadRequestId = manager->downloadModel(std::move(repoId), std::move(path));
            appState = std::move(nextState);
        };

        auto retryDownloadJob = [requestRemoteDownload](std::string repoId, std::string filePath) {
            requestRemoteDownload(std::move(repoId), std::move(filePath));
        };

        auto cancelDownloadJob = [appState, catalog, manager](std::string const &jobId) {
            if (jobId.empty()) {
                return;
            }
            AppState nextState = *appState;
            auto const it = std::find_if(nextState.recentDownloadJobs.begin(), nextState.recentDownloadJobs.end(), [&](DownloadJob const &job) {
                return job.id == jobId;
            });
            if (it == nextState.recentDownloadJobs.end()) {
                return;
            }

            bool const isActive = nextState.pendingDownloadJobId == jobId;
            std::string const filePath = it->filePath;
            removeDownloadJobFromState(nextState, jobId);
            if (isActive) {
                nextState.downloadingModel = false;
                nextState.pendingDownloadJobId.clear();
                nextState.pendingDownloadRepoId.clear();
                nextState.pendingDownloadFilePath.clear();
                nextState.latestDownloadRequestId = manager->cancelDownload();
                nextState.statusText = filePath.empty() ? "Download stopped" : "Stopped " + filePath;
            }
            try {
                catalog->deleteDownloadJob(jobId);
            } catch (std::exception const &e) {
                nextState.errorText = e.what();
            }
            appState = std::move(nextState);
        };

        auto removeDownloadJob = [appState, catalog](std::string const &jobId) {
            if (jobId.empty()) {
                return;
            }
            AppState nextState = *appState;
            removeDownloadJobFromState(nextState, jobId);
            try {
                catalog->deleteDownloadJob(jobId);
            } catch (std::exception const &e) {
                nextState.errorText = e.what();
            }
            appState = std::move(nextState);
        };

        auto requestModelLoad = [appState, manager, engine](std::string const &path, std::string const &name) {
            if (path.empty()) {
                return;
            }
            AppState nextState = *appState;
            std::string const displayName = !name.empty() ? name : modelDisplayName(path);
            if (nextState.modelLoading && nextState.pendingModelPath == path) {
                nextState.statusText = displayName.empty() ? "Loading selected model..." : "Loading " + displayName;
                nextState.errorText.clear();
            } else if (nextState.loadedModelPath == path && engine->isLoaded()) {
                nextState.statusText = displayName.empty() ? "Model already loaded" : "Loaded " + displayName;
                nextState.errorText.clear();
            } else if (hasActiveGenerations(nextState) && nextState.loadedModelPath != path) {
                nextState.deferredModelPath = path;
                nextState.deferredModelName = displayName;
                nextState.statusText = displayName.empty()
                                           ? "Will load the selected model after active responses finish"
                                           : "Will load " + displayName + " after active responses finish";
                nextState.errorText.clear();
            } else {
                requestModelLoadNow(nextState, *manager, path, name);
            }
            appState = std::move(nextState);
        };

        auto requestModelDelete = [appState, manager](std::string const &path, std::string const &repo, std::string const &name) {
            if (path.empty()) {
                return;
            }
            AppState nextState = *appState;
            std::string const displayName = !name.empty() ? name : modelDisplayName(path);
            nextState.modelDeleting = true;
            nextState.pendingDeleteModelPath = path;
            nextState.statusText = displayName.empty() ? "Deleting model..." : "Deleting " + displayName;
            nextState.errorText.clear();
            nextState.latestDeleteModelRequestId = manager->deleteModel(path, repo);
            appState = std::move(nextState);
        };

        auto selectChatModel = [appState, catalog](int chatIndex, std::string const &path, std::string const &name) {
            AppState nextState = *appState;
            setChatModel(nextState, chatIndex, path, name);
            persistChatThread(nextState, *catalog, chatIndex);
            std::string const displayName = !name.empty() ? name : modelDisplayName(path);
            nextState.statusText = displayName.empty() ? "Selected model" : "Selected " + displayName;
            nextState.errorText.clear();
            appState = std::move(nextState);
        };

        auto sendMessage = [appState, catalog, engine, manager](int chatIndex, std::string const &message) {
            if (message.empty()) {
                return;
            }
            AppState nextState = *appState;
            if (chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= nextState.chats.size()) {
                return;
            }

            ChatThread &chat = nextState.chats[static_cast<std::size_t>(chatIndex)];
            bool const fakeStreaming = lambda_studio_backend::debugFakeStreamEnabled();
            if (chat.streaming || (!fakeStreaming && chat.modelPath.empty())) {
                return;
            }

            if (startChatGeneration(nextState, *catalog, *engine, chatIndex, message)) {
                appState = std::move(nextState);
                return;
            }

            queuePendingChatSend(nextState, chat, message);
            if (!nextState.modelLoading && !hasActiveGenerations(nextState)) {
                requestModelLoadNow(nextState, *manager, chat.modelPath, chat.modelName);
            } else if (!nextState.modelLoading && nextState.loadedModelPath != chat.modelPath) {
                std::string const displayName = !chat.modelName.empty() ? chat.modelName : modelDisplayName(chat.modelPath);
                nextState.deferredModelPath = chat.modelPath;
                nextState.deferredModelName = displayName;
                nextState.statusText = displayName.empty()
                                           ? "Waiting for the active response to finish before loading the selected model"
                                           : "Waiting to load " + displayName + " after the active response finishes";
                nextState.errorText.clear();
            } else if (nextState.modelLoading && nextState.pendingModelPath == chat.modelPath) {
                std::string const displayName = !chat.modelName.empty() ? chat.modelName : modelDisplayName(chat.modelPath);
                nextState.statusText = displayName.empty() ? "Loading selected model..." : "Loading " + displayName;
                nextState.errorText.clear();
            } else {
                nextState.statusText = "Queued message until the selected model is ready";
                nextState.errorText.clear();
            }
            appState = std::move(nextState);
        };

        auto didAutoStream = useState(false);
        if (lambda_studio_backend::debugAutoStreamEnabled() && !(*didAutoStream) && !state.chats.empty()) {
            didAutoStream = true;
            Application::instance().onNextFrameNeeded([sendMessage] {
                sendMessage(0, "Run the markdown stress stream.");
            });
        }

        auto stopMessage = [appState, catalog, engine](int chatIndex) {
            AppState nextState = *appState;
            if (chatIndex >= 0 && static_cast<std::size_t>(chatIndex) < nextState.chats.size()) {
                ChatThread &chat = nextState.chats[static_cast<std::size_t>(chatIndex)];
                engine->cancelChat(chat.id);
                chat.streaming = false;
                chat.activeGenerationId = 0;
                persistChatThread(nextState, *catalog, chatIndex);
            }
            nextState.statusText = "Generation stopped";
            appState = std::move(nextState);
        };

        auto toggleReasoningMessage = [appState, catalog](int chatIndex, int messageIndex) {
            AppState nextState = *appState;
            if (chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= nextState.chats.size()) {
                return;
            }
            ChatThread &chat = nextState.chats[static_cast<std::size_t>(chatIndex)];
            if (messageIndex < 0) {
                return;
            }
            std::size_t const index = static_cast<std::size_t>(messageIndex);
            lambda::ChatMessage *message = nullptr;
            if (index < chat.messages.size()) {
                message = &chat.messages[index];
            } else {
                std::size_t const draftIndex = index - chat.messages.size();
                if (draftIndex < chat.streamDraftMessages.size()) {
                    message = &chat.streamDraftMessages[draftIndex];
                }
            }
            if (message == nullptr) {
                return;
            }
            if (message->role != ChatRole::Reasoning && message->role != ChatRole::Tool) {
                return;
            }
            message->collapsed = !message->collapsed;
            persistChatThread(nextState, *catalog, chatIndex);
            appState = std::move(nextState);
        };

        auto deleteChatMessage = [appState, catalog](int chatIndex, int messageIndex) {
            AppState nextState = *appState;
            if (chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= nextState.chats.size()) {
                return;
            }
            if (messageIndex < 0) {
                return;
            }
            ChatThread &chat = nextState.chats[static_cast<std::size_t>(chatIndex)];
            if (!eraseChatMessageByIndex(chat, static_cast<std::size_t>(messageIndex))) {
                return;
            }
            chat.updatedAtUnixMs = currentUnixMillis();
            persistChatThread(nextState, *catalog, chatIndex);
            nextState.statusText = "Deleted message";
            appState = std::move(nextState);
        };

        auto respondToToolApproval = [appState, catalog, engine](int chatIndex, int messageIndex, bool approved) {
            AppState nextState = *appState;
            lambda_studio_backend::debugToolTrace(
                string_format(
                    "ui.respondToToolApproval chat_index=%d message_index=%d approved=%d",
                    chatIndex,
                    messageIndex,
                    approved ? 1 : 0
                )
            );
            if (chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= nextState.chats.size() || messageIndex < 0) {
                lambda_studio_backend::debugToolTrace("ui.respondToToolApproval invalid_index");
                return;
            }

            ChatThread &chat = nextState.chats[static_cast<std::size_t>(chatIndex)];
            ChatMessage *message = chatMessageByIndex(chat, static_cast<std::size_t>(messageIndex));
            if (message == nullptr || message->role != ChatRole::Tool || message->toolCallId.empty()) {
                lambda_studio_backend::debugToolTrace("ui.respondToToolApproval no_pending_tool_message");
                nextState.errorText = "No pending tool approval for that message";
                appState = std::move(nextState);
                return;
            }

            if (chat.activeGenerationId == 0) {
                lambda_studio_backend::debugToolTrace(
                    string_format(
                        "ui.respondToToolApproval expired tool_call_id=%s",
                        message->toolCallId.c_str()
                    )
                );
                if (message->toolState == ToolMessageState::PendingApproval) {
                    message->toolState = ToolMessageState::Failed;
                    if (message->text.empty()) {
                        message->text = "{\"ok\":false,\"error\":\"pending tool approval is no longer active\"}";
                        ++message->textRevision;
                    }
                    chat.updatedAtUnixMs = currentUnixMillis();
                    persistChatThread(nextState, *catalog, chatIndex);
                }
                nextState.statusText = "Tool approval expired";
                nextState.errorText = "This tool approval is no longer active";
                appState = std::move(nextState);
                return;
            }

            lambda_studio_backend::debugToolTrace(
                string_format(
                    "ui.respondToToolApproval dispatch chat_id=%s generation_id=%llu tool_call_id=%s current_state=%d",
                    chat.id.c_str(),
                    static_cast<unsigned long long>(chat.activeGenerationId),
                    message->toolCallId.c_str(),
                    static_cast<int>(message->toolState)
                )
            );
            message->toolState = approved ? ToolMessageState::Running : ToolMessageState::Denied;
            if (approved) {
                if (message->text.empty()) {
                    message->text = "Waiting for tool execution...";
                    ++message->textRevision;
                }
            } else if (message->text.empty()) {
                message->text = "{\"ok\":false,\"error\":\"tool execution denied by user\"}";
                ++message->textRevision;
            }
            engine->respondToToolApproval(chat.id, chat.activeGenerationId, message->toolCallId, approved);
            chat.updatedAtUnixMs = currentUnixMillis();
            persistChatThread(nextState, *catalog, chatIndex);
            nextState.statusText = approved ? "Approved tool call" : "Denied tool call";
            nextState.errorText.clear();
            appState = std::move(nextState);
        };

        auto updateChatGenerationDefaults = [appState, catalog, engine](
                                                int chatIndex,
                                                lambda_studio_backend::GenerationParamsPatch const &patch
                                            ) {
            AppState nextState = *appState;
            if (chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= nextState.chats.size()) {
                return;
            }
            ChatThread &chat = nextState.chats[static_cast<std::size_t>(chatIndex)];
            lambda_studio_backend::GenerationParams params =
                chat.generationDefaults.value_or(nextState.generationDefaults);
            applyGenerationPatch(params, patch);
            chat.generationDefaults = params;

            lambda_studio_backend::ApplyResult const result =
                engine->updateChatGenerationParams(chat.id, patchFromGenerationParams(params));
            if (result.scope == lambda_studio_backend::ApplyScope::Rejected) {
                nextState.errorText = result.message.empty() ? "Invalid generation setting" : result.message;
            } else {
                try {
                    catalog->updateChatThreadGenerationDefaults(chat.id, chat.generationDefaults);
                } catch (std::exception const &e) {
                    nextState.errorText = e.what();
                }
                nextState.statusText = applyScopeLabel(result.scope) +
                                       (result.message.empty() ? std::string() : (": " + result.message));
                nextState.errorText.clear();
            }
            appState = std::move(nextState);
        };

        auto saveEngineDefaults = [catalog](AppState &state) {
            try {
                catalog->saveEngineConfigDefaults(lambda_studio_backend::EngineConfigDefaults {
                    .loadDefaults = state.loadDefaults,
                    .sessionDefaults = state.sessionDefaults,
                    .generationDefaults = state.generationDefaults,
                });
            } catch (std::exception const &e) {
                state.errorText = e.what();
            }
        };

        auto updateEngineGenerationDefaults = [appState, engine, saveEngineDefaults](lambda_studio_backend::GenerationParamsPatch const &patch) {
            AppState nextState = *appState;
            lambda_studio_backend::ApplyResult const result = engine->updateGenerationDefaults(patch);
            if (result.scope == lambda_studio_backend::ApplyScope::Rejected) {
                nextState.errorText = result.message.empty() ? "Invalid generation defaults" : result.message;
            } else {
                applyGenerationPatch(nextState.generationDefaults, patch);
                saveEngineDefaults(nextState);
                nextState.statusText = applyScopeLabel(result.scope) +
                                       (result.message.empty() ? std::string() : (": " + result.message));
                nextState.errorText.clear();
            }
            appState = std::move(nextState);
        };

        auto updateEngineSessionDefaults = [appState, engine, saveEngineDefaults](lambda_studio_backend::SessionParamsPatch const &patch) {
            AppState nextState = *appState;
            lambda_studio_backend::ApplyResult const result = engine->updateSessionDefaults(patch);
            if (result.scope == lambda_studio_backend::ApplyScope::Rejected) {
                nextState.errorText = result.message.empty() ? "Invalid session defaults" : result.message;
            } else {
                applySessionPatch(nextState.sessionDefaults, patch);
                saveEngineDefaults(nextState);
                nextState.statusText = applyScopeLabel(result.scope) +
                                       (result.message.empty() ? std::string() : (": " + result.message));
                nextState.errorText.clear();
            }
            appState = std::move(nextState);
        };

        auto updateEngineLoadDefaults = [appState, engine, saveEngineDefaults](lambda_studio_backend::LoadParamsPatch const &patch) {
            AppState nextState = *appState;
            lambda_studio_backend::ApplyResult const result = engine->updateLoadParams(patch);
            if (result.scope == lambda_studio_backend::ApplyScope::Rejected) {
                nextState.errorText = result.message.empty() ? "Invalid load defaults" : result.message;
            } else {
                applyLoadPatch(nextState.loadDefaults, patch);
                nextState.loadedModelPath = engine->modelPath();
                saveEngineDefaults(nextState);
                nextState.statusText = applyScopeLabel(result.scope) +
                                       (result.message.empty() ? std::string() : (": " + result.message));
                nextState.errorText.clear();
            }
            appState = std::move(nextState);
        };

        auto createChat = [appState, catalog]() {
            AppState nextState = *appState;
            ChatThread thread;
            thread.id = lambda::generateChatId();
            thread.title = "New chat";
            thread.updatedAtUnixMs = currentUnixMillis();
            thread.modelPath = nextState.loadedModelPath;
            thread.modelName = nextState.loadedModelName;
            nextState.chats.push_back(std::move(thread));
            nextState.selectedChatIndex = static_cast<int>(nextState.chats.size() - 1);
            persistChatThread(nextState, *catalog, nextState.selectedChatIndex, true);
            persistChatOrder(nextState, *catalog);
            appState = std::move(nextState);
        };

        auto deleteChat = [appState, catalog, engine](int chatIndex) {
            AppState nextState = *appState;
            if (chatIndex < 0 || static_cast<std::size_t>(chatIndex) >= nextState.chats.size()) {
                return;
            }

            ChatThread const removedChat = nextState.chats[static_cast<std::size_t>(chatIndex)];
            if (removedChat.streaming || removedChat.activeGenerationId != 0) {
                engine->cancelChat(removedChat.id);
            }
            clearPendingChatSendsForChat(nextState, removedChat.id);
            nextState.chats.erase(nextState.chats.begin() + chatIndex);
            if (nextState.chats.empty()) {
                nextState.selectedChatIndex = 0;
            } else {
                nextState.selectedChatIndex = std::min(chatIndex, static_cast<int>(nextState.chats.size() - 1));
            }

            try {
                catalog->deleteChatThread(removedChat.id);
            } catch (std::exception const &e) {
                nextState.errorText = e.what();
            }
            persistChatOrder(nextState, *catalog);
            persistSelectedChat(nextState, *catalog);
            nextState.statusText = "Deleted chat";
            appState = std::move(nextState);
        };

        Element currentView = ChatsView {
            .state = state,
            .onNewChat = createChat,
            .onSelectChat = [appState, catalog](int index) {
                AppState nextState = *appState;
                nextState.selectedChatIndex = index;
                persistSelectedChat(nextState, *catalog);
                appState = std::move(nextState);
            },
            .onSelectModel = selectChatModel,
            .onSend = sendMessage,
            .onStop = stopMessage,
            .onDeleteChat = deleteChat,
            .onToggleReasoning = toggleReasoningMessage,
            .onDeleteMessage = deleteChatMessage,
            .onApproveTool = [respondToToolApproval](int chatIndex, int messageIndex) {
                printf("tool approved %d %d\n", chatIndex, messageIndex);
                respondToToolApproval(chatIndex, messageIndex, true);
            },
            .onDenyTool = [respondToToolApproval](int chatIndex, int messageIndex) {
                respondToToolApproval(chatIndex, messageIndex, false);
            },
            .onAdjustGeneration = updateChatGenerationDefaults,
        }
                                  .flex(1.f, 1.f);

        if (state.currentModule == StudioModule::Models) {
            currentView = ModelsView {
                .state = state,
                .onRefresh = requestInventoryRefresh,
                .onLoad = requestModelLoad,
                .onDeleteModel = requestModelDelete,
                .onRetryDownload = retryDownloadJob,
                .onCancelDownload = cancelDownloadJob,
                .onRemoveDownload = removeDownloadJob,
            }
                              .flex(1.f, 1.f);
        } else if (state.currentModule == StudioModule::Hub) {
            currentView = HubView {
                .state = state,
                .onSearchQueryChange = updateModelSearchQuery,
                .onSortChange = updateRemoteModelSort,
                .onVisibilityChange = updateRemoteModelVisibility,
                .onSearch = requestRemoteSearch,
                .onSelectRemoteRepo = requestRemoteRepoFiles,
                .onDownload = requestRemoteDownload,
                .onCancelDownload = cancelDownloadJob,
            }
                              .flex(1.f, 1.f);
        } else if (state.currentModule == StudioModule::Settings) {
            currentView = SettingsView {
                .state = state,
                .onAdjustGenerationDefaults = updateEngineGenerationDefaults,
                .onAdjustSessionDefaults = updateEngineSessionDefaults,
                .onAdjustLoadDefaults = updateEngineLoadDefaults,
            }
                              .flex(1.f, 1.f);
        }

        return HStack {
            .spacing = 0.f,
            .alignment = Alignment::Stretch,
            .children = children(
                Sidebar {
                    .modules = {
                        {IconName::ChatBubble, "Chats"},
                        {IconName::ModelTraining, "Models"},
                        {IconName::Cloud, "Hub"},
                        {IconName::Settings, "Settings"}
                    },
                    .selectedTitle = moduleTitle(state.currentModule),
                    .onSelect = [appState](std::string title) {
                        AppState nextState = *appState;
                        nextState.currentModule = moduleFromTitle(title);
                        appState = std::move(nextState);
                    },
                }
                    .flex(0.f, 0.f),
                Divider {.orientation = Divider::Orientation::Vertical},
                VStack {
                    .spacing = theme().space3,
                    .alignment = Alignment::Stretch,
                    .children = children(std::move(currentView))
                }
                    .flex(1.f, 1.f)
            ),
        };
    }
};

int main(int argc, char *argv[]) {
    Application app(argc, argv);
    std::shared_ptr<AppRuntime> runtime =
        makeAppRuntime(makeDefaultAppRuntimeFactory(
            [](lambda_studio_backend::ModelManagerEvent ev) {
                if (!Application::hasInstance()) {
                    return;
                }
                Application::instance().eventQueue().post(std::move(ev));
            }
        ));

    auto &w = app.createWindow<Window>({
        .size = {800, 800},
        .title = "Lambda Studio",
        .resizable = true,
    });

    w.setView<StudioApp>({.runtime = std::move(runtime)});

    return app.exec();
}
