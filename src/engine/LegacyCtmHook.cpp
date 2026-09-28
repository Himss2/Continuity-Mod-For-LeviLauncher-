#include "engine/LegacyCtmHook.h"
#include "engine/CtmResolver.h"
#include <pl/memory/Hook.hpp>

namespace continuity_bedrock::engine {
LegacyCtmHook* LegacyCtmHook::sInstance = nullptr;
static constexpr ptrdiff_t kBlockTessellatorCacheOffset_1_26_52_3 = 0x678;

bool LegacyCtmHook::install(const Addresses& a, const ModConfig& cfg, LogFn log) {
    uninstall();
    mAddresses = a; mConfig = cfg; mLog = std::move(log); sInstance = this;
    mCacheGetBlock = reinterpret_cast<CacheGetBlockFn>(a.blockTessellatorCacheGetBlock);
    mGetTextureUv = reinterpret_cast<AtlasCache::GetTextureUvFn>(a.blockGraphicsGetTextureUv);
    if (a.blockTessellatorGetTexture) {
        mGetTextureInstalled = pl::memory::hook(
            reinterpret_cast<void*>(a.blockTessellatorGetTexture),
            reinterpret_cast<void*>(&LegacyCtmHook::getTextureDetour),
            reinterpret_cast<void**>(&mOriginalGetTexture)) == 0;
    }
    if (cfg.diagnostics && a.useNewTessellation) {
        mUseNewInstalled = pl::memory::hook(
            reinterpret_cast<void*>(a.useNewTessellation),
            reinterpret_cast<void*>(&LegacyCtmHook::useNewDetour),
            reinterpret_cast<void**>(&mOriginalUseNew)) == 0;
    }
    return mGetTextureInstalled;
}

void LegacyCtmHook::uninstall() {
    if (mGetTextureInstalled && mAddresses.blockTessellatorGetTexture)
        pl::memory::unhook(reinterpret_cast<void*>(mAddresses.blockTessellatorGetTexture), reinterpret_cast<void*>(&LegacyCtmHook::getTextureDetour));
    if (mUseNewInstalled && mAddresses.useNewTessellation)
        pl::memory::unhook(reinterpret_cast<void*>(mAddresses.useNewTessellation), reinterpret_cast<void*>(&LegacyCtmHook::useNewDetour));
    mGetTextureInstalled = false; mUseNewInstalled = false;
    mOriginalGetTexture = nullptr; mOriginalUseNew = nullptr;
    if (sInstance == this) sInstance = nullptr;
}

const TextureUvSet* LegacyCtmHook::getTextureDetour(void* self, const BlockPos* pos, const void* block,
                                                     uint8_t face, int forcedVariant, const void* graphics) {
    return sInstance ? sInstance->onGetTexture(self, pos, block, face, forcedVariant, graphics) : nullptr;
}
bool LegacyCtmHook::useNewDetour(const void* block, bool onlyNew) {
    return sInstance ? sInstance->onUseNew(block, onlyNew) : false;
}

const TextureUvSet* LegacyCtmHook::onGetTexture(void* self, const BlockPos* pos, const void* block,
                                                uint8_t face, int forcedVariant, const void* graphics) {
    const auto* original = mOriginalGetTexture(self, pos, block, face, forcedVariant, graphics);
    const auto callCount = mGetTextureCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (!original || !pos || !block) return original;

    const bool shouldProbe = callCount == 1 || (callCount % 4096u) == 0;
    if ((mConfig.atlasProbe || mConfig.enableLegacyCtm) && !mAtlas.ready() && shouldProbe) {
        if (mAtlas.ensureLoaded(mGetTextureUv)) {
            if (!mLoggedAtlasReady.exchange(true) && mLog) mLog("Atlas probe ready: glass + 47 CTM tiles resolved");
        } else if (!mLoggedAtlasFailure.exchange(true) && mLog) {
            mLog(std::string("Atlas probe not ready: ") + mAtlas.lastError());
        }
    }
    if (!mAtlas.matchesVanillaGlass(*original)) return original;

    mGlassLegacyHits.fetch_add(1, std::memory_order_relaxed);
    if (!mLoggedFirstGlass.exchange(true) && mLog) mLog("minecraft:glass reached legacy BlockTessellator::_getTexture path");
    if (!mConfig.enableLegacyCtm || !mCacheGetBlock || face > 5) return original;

    auto* cache = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(self) + kBlockTessellatorCacheOffset_1_26_52_3);
    const uint8_t mask = ctm::buildMask(*pos, face, [&](const BlockPos& p) {
        // POC connect=state/block for vanilla glass: permutations are canonical pointers.
        // Generic connect=block/state is intentionally deferred until Block layout is runtime-validated.
        return mCacheGetBlock(cache, &p) == block;
    });
    return &mAtlas.tile(ctm::tileForMask(mask));
}

bool LegacyCtmHook::onUseNew(const void* block, bool onlyNew) {
    const bool result = mOriginalUseNew(block, onlyNew);
    mPipelineChecks.fetch_add(1, std::memory_order_relaxed);
    if (result) mPipelineTrue.fetch_add(1, std::memory_order_relaxed);
    else mPipelineFalse.fetch_add(1, std::memory_order_relaxed);
    return result;
}

HookStats LegacyCtmHook::stats() const {
    return {mGetTextureCalls.load(), mGlassLegacyHits.load(), mPipelineChecks.load(), mPipelineTrue.load(), mPipelineFalse.load()};
}
}
