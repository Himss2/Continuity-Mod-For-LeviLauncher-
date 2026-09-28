#include "engine/LegacyCtmHook.h"
#include <pl/memory/Hook.hpp>

namespace continuity_bedrock::engine {
LegacyCtmHook* LegacyCtmHook::sInstance = nullptr;

bool LegacyCtmHook::install(const Addresses& a, const ModConfig& cfg, LogFn log) {
    uninstall();
    mAddresses = a;
    mConfig = cfg;
    mLog = std::move(log);
    mGetTextureCalls.store(0, std::memory_order_relaxed);
    mPipelineChecks.store(0, std::memory_order_relaxed);
    mPipelineTrue.store(0, std::memory_order_relaxed);
    mPipelineFalse.store(0, std::memory_order_relaxed);
    mDiagnosticLogCount.store(0, std::memory_order_relaxed);
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

    if (cfg.diagnostics) {
        logDiagnostic(
            std::string("Hook install state: _getTexture=")
            + (mGetTextureInstalled ? "installed" : "FAILED")
            + ", useNewTessellation="
            + (mUseNewInstalled ? "installed" : "not-installed"));
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

void LegacyCtmHook::logDiagnostic(std::string message) {
    if (!mConfig.diagnostics || !mLog || mConfig.maxDiagnosticLogs <= 0) return;

    const uint32_t limit = static_cast<uint32_t>(mConfig.maxDiagnosticLogs);
    uint32_t current = mDiagnosticLogCount.load(std::memory_order_relaxed);
    while (current < limit) {
        if (mDiagnosticLogCount.compare_exchange_weak(
                current,
                current + 1,
                std::memory_order_relaxed,
                std::memory_order_relaxed)) {
            mLog(message);
            return;
        }
    }
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
    const uint64_t count =
        mGetTextureCalls.fetch_add(1, std::memory_order_relaxed) + 1;

    if (count == 1) {
        std::string message =
            "RUNTIME HIT: legacy BlockTessellator::_getTexture detour is executing";
        if (pos) {
            message += "; pos=("
                + std::to_string(pos->x) + ","
                + std::to_string(pos->y) + ","
                + std::to_string(pos->z) + ")";
        }
        message += "; face=" + std::to_string(static_cast<unsigned>(face));
        logDiagnostic(std::move(message));
    } else if ((count & 0xFFFFu) == 0) {
        const auto s = stats();
        logDiagnostic(
            "RUNTIME STATS: getTexture=" + std::to_string(s.getTextureCalls)
            + ", pipelineChecks=" + std::to_string(s.pipelineChecks)
            + ", newTrue=" + std::to_string(s.pipelineTrue)
            + ", newFalse=" + std::to_string(s.pipelineFalse));
    }

    // Diagnostic-only by design. No block is matched and no UV is replaced here.
    // In particular, glass is deliberately untouched so BedrockTools' Connected
    // Glass module remains the sole owner of glass/pane rendering.
    return mOriginalGetTexture(self, pos, block, face, forcedVariant, graphics);
}

bool LegacyCtmHook::onUseNew(const void* block, bool onlyNew) {
    const bool result = mOriginalUseNew(block, onlyNew);
    const uint64_t count =
        mPipelineChecks.fetch_add(1, std::memory_order_relaxed) + 1;

    if (result) {
        mPipelineTrue.fetch_add(1, std::memory_order_relaxed);
    } else {
        mPipelineFalse.fetch_add(1, std::memory_order_relaxed);
    }

    if (count == 1) {
        logDiagnostic(
            std::string("RUNTIME HIT: BlockTessellatorPipeline::useNewTessellation detour is executing")
            + "; onlyNew=" + (onlyNew ? "true" : "false")
            + "; result=" + (result ? "true" : "false"));
    } else if ((count & 0x3FFFu) == 0) {
        const auto s = stats();
        logDiagnostic(
            "RUNTIME STATS: getTexture=" + std::to_string(s.getTextureCalls)
            + ", pipelineChecks=" + std::to_string(s.pipelineChecks)
            + ", newTrue=" + std::to_string(s.pipelineTrue)
            + ", newFalse=" + std::to_string(s.pipelineFalse));
    }

    return result;
}

HookStats LegacyCtmHook::stats() const {
    return {
        mGetTextureCalls.load(std::memory_order_relaxed),
        mPipelineChecks.load(std::memory_order_relaxed),
        mPipelineTrue.load(std::memory_order_relaxed),
        mPipelineFalse.load(std::memory_order_relaxed)
    };
}
}
