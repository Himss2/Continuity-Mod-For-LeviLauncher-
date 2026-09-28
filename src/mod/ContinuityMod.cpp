#include "mod/ContinuityMod.h"
#include "engine/Signatures.h"
#include <filesystem>
#include <fmt/format.h>

namespace continuity_bedrock {
ContinuityMod& ContinuityMod::instance() {
    static ContinuityMod i;
    return i;
}

ContinuityMod::ContinuityMod() : mSelf(*ll::mod::NativeMod::current()) {}

bool ContinuityMod::load() {
    auto& log = getSelf().getLogger();

    std::error_code ec;
    std::filesystem::create_directories(getSelf().getConfigDir(), ec);

    mConfigFile.emplace();
    if (!mConfigFile->load()) {
        log.error("Failed to load config");
        return false;
    }
    mConfig = mConfigFile->value();

    log.info(
        "Loaded Continuity Bedrock diagnostic core. Connected glass is intentionally excluded "
        "and delegated to BedrockTools.");

    return true;
}

bool ContinuityMod::enable() {
    auto& log = getSelf().getLogger();
    if (!mConfig.enabled) {
        log.info("Disabled by config");
        return true;
    }

    mAddresses = engine::resolveAddresses();
    log.info(
        "RE 1.26.52.3: _getTexture={:#x}, cacheGetBlock={:#x}, atlasUv={:#x}, useNew={:#x}",
        mAddresses.blockTessellatorGetTexture,
        mAddresses.blockTessellatorCacheGetBlock,
        mAddresses.blockGraphicsGetTextureUv,
        mAddresses.useNewTessellation);

    if (!mAddresses.blockTessellatorGetTexture) {
        log.error("Legacy renderer diagnostic signature did not resolve. Hooks are NOT installed.");
        return true;
    }

    const bool ok = mHook.install(
        mAddresses,
        mConfig,
        [&](const std::string& s) { getSelf().getLogger().info("{}", s); });

    if (!ok) {
        log.error("Failed to install legacy _getTexture diagnostic hook");
    } else {
        log.info("Renderer diagnostics installed; no texture replacement is active.");
    }

    return true;
}

bool ContinuityMod::disable() {
    const auto s = mHook.stats();
    getSelf().getLogger().info(
        "Renderer stats: getTexture={}, pipelineChecks={}, newTrue={}, newFalse={}",
        s.getTextureCalls,
        s.pipelineChecks,
        s.pipelineTrue,
        s.pipelineFalse);

    mHook.uninstall();
    return true;
}

bool ContinuityMod::unload() {
    mConfigFile.reset();
    return true;
}
}
