#pragma once

#include "engine/BetterGrassResolver.h"
#include "engine/Signatures.h"
#include "engine/Types.h"
#include "mod/Config.h"

#include <atomic>
#include <functional>
#include <string>

namespace better_grass::engine {

struct HookStats {
    uint64_t getTextureCalls{};
    uint64_t replacedFaces{};
};

class BetterGrassHook {
public:
    using LogFn = std::function<void(const std::string&)>;

    bool install(const Addresses& addresses, const ModConfig& config, LogFn log);
    void uninstall();

    void setEnabled(bool enabled) {
        mEnabled.store(enabled, std::memory_order_release);
    }

    bool enabled() const {
        return mEnabled.load(std::memory_order_acquire);
    }

    HookStats stats() const;

private:
    using GetTextureFn =
        const TextureUvSet* (*)(void*, const BlockPos*, const void*, uint8_t, int, const void*);

    static const TextureUvSet* getTextureDetour(
        void*, const BlockPos*, const void*, uint8_t, int, const void*);

    const TextureUvSet* onGetTexture(
        void*, const BlockPos*, const void*, uint8_t, int, const void*);

    static BetterGrassHook* sInstance;

    ModConfig mConfig{};
    LogFn mLog;
    Addresses mAddresses{};
    GetTextureFn mOriginalGetTexture{};
    BetterGrassResolver::CacheGetBlockFn mCacheGetBlock{};
    BetterGrassResolver mResolver{};
    bool mInstalled{};

    std::atomic<uint64_t> mGetTextureCalls{};
    std::atomic<uint64_t> mReplacedFaces{};
    std::atomic_bool mLoggedFirstHit{};
    std::atomic_bool mLoggedFirstReplacement{};
    std::atomic_bool mEnabled{true};
};

}
