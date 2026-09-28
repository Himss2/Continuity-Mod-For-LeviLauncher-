#include "engine/LegacyCtmHook.h"

#include <pl/memory/Hook.hpp>

namespace continuity_bedrock::engine {

LegacyCtmHook* LegacyCtmHook::sInstance = nullptr;

namespace {
constexpr ptrdiff_t kBlockTessellatorCacheOffset_1_26_52_3 = 0x678;
}

bool LegacyCtmHook::install(
    const Addresses& addresses,
    const ModConfig& config,
    RuleEngine* rules,
    LogFn log
) {
    uninstall();

    mAddresses = addresses;
    mConfig = config;
    mRules = rules;
    mLog = std::move(log);
    mCacheGetBlock =
        reinterpret_cast<RuleEngine::CacheGetBlockFn>(addresses.blockTessellatorCacheGetBlock);
    mGetTextureUv =
        reinterpret_cast<RuleEngine::GetTextureUvFn>(addresses.blockGraphicsGetTextureUv);

    mGetTextureCalls.store(0, std::memory_order_relaxed);
    mReplacedFaces.store(0, std::memory_order_relaxed);
    mLoggedFirstHit.store(false, std::memory_order_relaxed);

    sInstance = this;

    if (addresses.blockTessellatorGetTexture) {
        mInstalled = pl::memory::hook(
            reinterpret_cast<void*>(addresses.blockTessellatorGetTexture),
            reinterpret_cast<void*>(&LegacyCtmHook::getTextureDetour),
            reinterpret_cast<void**>(&mOriginalGetTexture)) == 0;
    }

    if (mConfig.diagnostics && mLog) {
        mLog(
            std::string("Hook install state: _getTexture=")
            + (mInstalled ? "installed" : "FAILED")
            + "; compiledRules="
            + std::to_string(mRules ? mRules->ruleCount() : 0));
    }

    return mInstalled;
}

void LegacyCtmHook::uninstall() {
    if (mInstalled && mAddresses.blockTessellatorGetTexture) {
        pl::memory::unhook(
            reinterpret_cast<void*>(mAddresses.blockTessellatorGetTexture),
            reinterpret_cast<void*>(&LegacyCtmHook::getTextureDetour));
    }

    mInstalled = false;
    mOriginalGetTexture = nullptr;
    mRules = nullptr;
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

const TextureUvSet* LegacyCtmHook::onGetTexture(
    void* self,
    const BlockPos* pos,
    const void* block,
    uint8_t face,
    int forcedVariant,
    const void* graphics
) {
    const auto* original =
        mOriginalGetTexture(self, pos, block, face, forcedVariant, graphics);

    const uint64_t callCount =
        mGetTextureCalls.fetch_add(1, std::memory_order_relaxed) + 1;

    if (mConfig.diagnostics
        && !mLoggedFirstHit.exchange(true, std::memory_order_relaxed)
        && mLog) {
        mLog("RUNTIME HIT: generic Continuity rule engine is on legacy _getTexture");
    }

    if (!original
        || !self
        || !pos
        || !block
        || !mConfig.enableRuleEngine
        || !mRules
        || !mCacheGetBlock
        || !mGetTextureUv) {
        return original;
    }

    auto* blockCache = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(self) + kBlockTessellatorCacheOffset_1_26_52_3);

    const auto* replacement = mRules->process(
        blockCache,
        *pos,
        block,
        face,
        *original,
        mCacheGetBlock,
        mGetTextureUv,
        callCount);

    if (replacement != original) {
        mReplacedFaces.fetch_add(1, std::memory_order_relaxed);
    }
    return replacement;
}

HookStats LegacyCtmHook::stats() const {
    return {
        mGetTextureCalls.load(std::memory_order_relaxed),
        mReplacedFaces.load(std::memory_order_relaxed)
    };
}

}
