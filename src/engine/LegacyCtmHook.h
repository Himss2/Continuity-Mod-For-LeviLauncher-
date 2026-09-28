#pragma once
#include "engine/Signatures.h"
#include "engine/Types.h"
#include "mod/Config.h"
#include <atomic>
#include <functional>
#include <string>

namespace continuity_bedrock::engine {
struct HookStats {
    uint64_t getTextureCalls{};
    uint64_t pipelineChecks{};
    uint64_t pipelineTrue{};
    uint64_t pipelineFalse{};
};

class LegacyCtmHook {
public:
    using LogFn = std::function<void(const std::string&)>;

    bool install(const Addresses& a, const ModConfig& cfg, LogFn log);
    void uninstall();
    HookStats stats() const;

private:
    using GetTextureFn = const TextureUvSet* (*)(void*, const BlockPos*, const void*, uint8_t, int, const void*);
    using UseNewTessellationFn = bool (*)(const void*, bool);

    static const TextureUvSet* getTextureDetour(
        void*, const BlockPos*, const void*, uint8_t, int, const void*);
    static bool useNewDetour(const void*, bool);

    const TextureUvSet* onGetTexture(
        void*, const BlockPos*, const void*, uint8_t, int, const void*);
    bool onUseNew(const void*, bool);

    static LegacyCtmHook* sInstance;
    ModConfig mConfig{};
    LogFn mLog;
    Addresses mAddresses{};
    GetTextureFn mOriginalGetTexture{};
    UseNewTessellationFn mOriginalUseNew{};
    bool mGetTextureInstalled{};
    bool mUseNewInstalled{};
    std::atomic<uint64_t> mGetTextureCalls{};
    std::atomic<uint64_t> mPipelineChecks{};
    std::atomic<uint64_t> mPipelineTrue{};
    std::atomic<uint64_t> mPipelineFalse{};
};
}
