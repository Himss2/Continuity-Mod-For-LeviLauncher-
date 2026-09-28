#pragma once

#include "engine/RuleEngine.h"
#include "engine/Signatures.h"
#include "engine/Types.h"
#include "mod/Config.h"

#include <atomic>
#include <functional>
#include <string>

namespace continuity_bedrock::engine {

struct HookStats {
    uint64_t getTextureCalls{};
    uint64_t replacedFaces{};
};

class LegacyCtmHook {
public:
    using LogFn = std::function<void(const std::string&)>;

    bool install(
        const Addresses& addresses,
        const ModConfig& config,
        RuleEngine* rules,
        LogFn log
    );
    void uninstall();
    HookStats stats() const;

private:
    using GetTextureFn =
        const TextureUvSet* (*)(void*, const BlockPos*, const void*, uint8_t, int, const void*);

    static const TextureUvSet* getTextureDetour(
        void*, const BlockPos*, const void*, uint8_t, int, const void*);

    const TextureUvSet* onGetTexture(
        void*, const BlockPos*, const void*, uint8_t, int, const void*);

    static LegacyCtmHook* sInstance;

    ModConfig mConfig{};
    LogFn mLog;
    Addresses mAddresses{};
    RuleEngine* mRules{};
    GetTextureFn mOriginalGetTexture{};
    RuleEngine::CacheGetBlockFn mCacheGetBlock{};
    RuleEngine::GetTextureUvFn mGetTextureUv{};
    bool mInstalled{};

    std::atomic<uint64_t> mGetTextureCalls{};
    std::atomic<uint64_t> mReplacedFaces{};
    std::atomic_bool mLoggedFirstHit{};
};

}
