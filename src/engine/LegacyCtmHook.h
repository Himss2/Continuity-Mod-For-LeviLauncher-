#pragma once

#include "engine/BetterGrassResolver.h"
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
    void setRuleEngineEnabled(bool enabled) {
        mRuleEngineEnabled.store(enabled, std::memory_order_release);
    }
    bool ruleEngineEnabled() const {
        return mRuleEngineEnabled.load(std::memory_order_acquire);
    }
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
    BetterGrassResolver mBetterGrass{};
    GetTextureFn mOriginalGetTexture{};
    RuleEngine::CacheGetBlockFn mCacheGetBlock{};
    RuleEngine::GetTextureUvFn mGetTextureUv{};
    bool mInstalled{};

    std::atomic<uint64_t> mGetTextureCalls{};
    std::atomic<uint64_t> mReplacedFaces{};
    std::atomic_bool mLoggedFirstHit{};
    std::atomic_bool mLoggedFirstBetterGrass{};
    std::atomic_bool mRuleEngineEnabled{true};
};

}
