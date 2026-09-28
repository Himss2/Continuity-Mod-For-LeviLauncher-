#include "engine/LegacyCtmHook.h"
#include <pl/memory/Hook.hpp>

namespace continuity_bedrock::engine {
LegacyCtmHook* LegacyCtmHook::sInstance = nullptr;

bool LegacyCtmHook::install(const Addresses& a, const ModConfig& cfg, LogFn log) {
    uninstall();
    mAddresses = a;
    mConfig = cfg;
    mLog = std::move(log);
    sInstance = this;

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
    if (mGetTextureInstalled && mAddresses.blockTessellatorGetTexture) {
        pl::memory::unhook(
            reinterpret_cast<void*>(mAddresses.blockTessellatorGetTexture),
            reinterpret_cast<void*>(&LegacyCtmHook::getTextureDetour));
    }
    if (mUseNewInstalled && mAddresses.useNewTessellation) {
        pl::memory::unhook(
            reinterpret_cast<void*>(mAddresses.useNewTessellation),
            reinterpret_cast<void*>(&LegacyCtmHook::useNewDetour));
    }

    mGetTextureInstalled = false;
    mUseNewInstalled = false;
    mOriginalGetTexture = nullptr;
    mOriginalUseNew = nullptr;
    if (sInstance == this) sInstance = nullptr;
}

const TextureUvSet* LegacyCtmHook::getTextureDetour(
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

bool LegacyCtmHook::useNewDetour(const void* block, bool onlyNew) {
    return sInstance ? sInstance->onUseNew(block, onlyNew) : false;
}

const TextureUvSet* LegacyCtmHook::onGetTexture(
    void* self,
    const BlockPos* pos,
    const void* block,
    uint8_t face,
    int forcedVariant,
    const void* graphics
) {
    mGetTextureCalls.fetch_add(1, std::memory_order_relaxed);

    // Diagnostic-only by design. No block is matched and no UV is replaced here.
    // In particular, glass is deliberately untouched so BedrockTools' Connected
    // Glass module remains the sole owner of glass/pane rendering.
    return mOriginalGetTexture(self, pos, block, face, forcedVariant, graphics);
}

bool LegacyCtmHook::onUseNew(const void* block, bool onlyNew) {
    const bool result = mOriginalUseNew(block, onlyNew);
    mPipelineChecks.fetch_add(1, std::memory_order_relaxed);
    if (result) {
        mPipelineTrue.fetch_add(1, std::memory_order_relaxed);
    } else {
        mPipelineFalse.fetch_add(1, std::memory_order_relaxed);
    }
    return result;
}

HookStats LegacyCtmHook::stats() const {
    return {
        mGetTextureCalls.load(),
        mPipelineChecks.load(),
        mPipelineTrue.load(),
        mPipelineFalse.load()
    };
}
}
