#include "engine/BetterGrassHook.h"

#include <pl/memory/Hook.hpp>

#include <utility>

namespace better_grass::engine {

BetterGrassHook* BetterGrassHook::sInstance = nullptr;

namespace {
constexpr ptrdiff_t kBlockTessellatorCacheOffset_1_26_52_3 = 0x678;
}

bool BetterGrassHook::install(
    const Addresses& addresses,
    const ModConfig& config,
    LogFn log
) {
    uninstall();

    mAddresses = addresses;
    mConfig = config;
    mLog = std::move(log);
    mCacheGetBlock =
        reinterpret_cast<BetterGrassResolver::CacheGetBlockFn>(
            addresses.blockTessellatorCacheGetBlock);

    mGetTextureCalls.store(0, std::memory_order_relaxed);
    mReplacedFaces.store(0, std::memory_order_relaxed);
    mLoggedFirstHit.store(false, std::memory_order_relaxed);
    mLoggedFirstReplacement.store(false, std::memory_order_relaxed);
    mEnabled.store(config.enabled, std::memory_order_relaxed);

    sInstance = this;

    if (addresses.blockTessellatorGetTexture) {
        mInstalled = pl::memory::hook(
            reinterpret_cast<void*>(addresses.blockTessellatorGetTexture),
            reinterpret_cast<void*>(&BetterGrassHook::getTextureDetour),
            reinterpret_cast<void**>(&mOriginalGetTexture)) == 0;
    }

    if (mConfig.diagnostics && mLog) {
        mLog(
            std::string("Better Grass hook: _getTexture=")
            + (mInstalled ? "installed" : "FAILED"));
    }

    return mInstalled;
}

void BetterGrassHook::uninstall() {
    if (mInstalled && mAddresses.blockTessellatorGetTexture) {
        pl::memory::unhook(
            reinterpret_cast<void*>(mAddresses.blockTessellatorGetTexture),
            reinterpret_cast<void*>(&BetterGrassHook::getTextureDetour));
    }

    mInstalled = false;
    mOriginalGetTexture = nullptr;
    mCacheGetBlock = nullptr;
    if (sInstance == this) sInstance = nullptr;
}

const TextureUvSet* BetterGrassHook::getTextureDetour(
    void* self,
    const BlockPos* pos,
    const void* block,
    uint8_t face,
    int forcedVariant,
    const void* graphics
) {
    return sInstance
        ? sInstance->onGetTexture(self, pos, block, face, forcedVariant, graphics)
        : nullptr;
}

const TextureUvSet* BetterGrassHook::onGetTexture(
    void* self,
    const BlockPos* pos,
    const void* block,
    uint8_t face,
    int forcedVariant,
    const void* graphics
) {
    const auto* original =
        mOriginalGetTexture(self, pos, block, face, forcedVariant, graphics);

    mGetTextureCalls.fetch_add(1, std::memory_order_relaxed);

    if (mConfig.diagnostics
        && !mLoggedFirstHit.exchange(true, std::memory_order_relaxed)
        && mLog) {
        mLog("RUNTIME HIT: Better Grass is on legacy BlockTessellator::_getTexture");
    }

    if (!original
        || !self
        || !pos
        || !block
        || !mCacheGetBlock
        || !mEnabled.load(std::memory_order_relaxed)) {
        return original;
    }

    auto* blockCache = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(self)
        + kBlockTessellatorCacheOffset_1_26_52_3);

    const auto* replacement = mResolver.process(
        blockCache,
        self,
        *pos,
        block,
        face,
        original,
        mCacheGetBlock,
        reinterpret_cast<BetterGrassResolver::GetTextureFn>(mOriginalGetTexture));

    if (replacement != original) {
        mReplacedFaces.fetch_add(1, std::memory_order_relaxed);

        if (mConfig.diagnostics
            && !mLoggedFirstReplacement.exchange(true, std::memory_order_relaxed)
            && mLog) {
            mLog(
                "BETTER GRASS HIT: Fancy diagonal-down match; "
                "side now uses Minecraft's live UP-face texture");
        }
    }

    return replacement;
}

HookStats BetterGrassHook::stats() const {
    return {
        mGetTextureCalls.load(std::memory_order_relaxed),
        mReplacedFaces.load(std::memory_order_relaxed)
    };
}

}
