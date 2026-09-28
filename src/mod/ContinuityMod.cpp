#include "mod/ContinuityMod.h"

#include "engine/Signatures.h"

#include <filesystem>
#include <fmt/format.h>

namespace continuity_bedrock {

ContinuityMod& ContinuityMod::instance() {
    static ContinuityMod instance;
    return instance;
}

ContinuityMod::ContinuityMod()
    : mSelf(*ll::mod::NativeMod::current()) {}

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

    const auto rulesDir = getSelf().getResourceDir() / "continuity" / "rules";
    const bool loaded = mRules.load(
        rulesDir,
        [&](const std::string& message) { getSelf().getLogger().info("{}", message); });

    log.info(
        "Loaded Continuity Bedrock rule engine; rules={}, glass delegated to BedrockTools",
        mRules.ruleCount());

    if (!loaded) {
        log.warn("No supported Continuity rules were compiled");
    }

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
        "RE 1.26.52.3: _getTexture={:#x}, cacheGetBlock={:#x}, atlasUv={:#x}",
        mAddresses.blockTessellatorGetTexture,
        mAddresses.blockTessellatorCacheGetBlock,
        mAddresses.blockGraphicsGetTextureUv);

    if (!engine::coreAddressesReady(mAddresses)) {
        log.error("Core renderer signatures did not resolve. Hooks are NOT installed.");
        return true;
    }

    const bool ok = mHook.install(
        mAddresses,
        mConfig,
        &mRules,
        [&](const std::string& message) { getSelf().getLogger().info("{}", message); });

    if (!ok) {
        log.error("Failed to install generic Continuity legacy hook");
    } else {
        log.info(
            "Generic rule hook active; rules={}, visualPOC={}",
            mRules.ruleCount(),
            mConfig.enableBookshelfPoc);
    }

    return true;
}

bool ContinuityMod::disable() {
    const auto stats = mHook.stats();
    getSelf().getLogger().info(
        "Renderer stats: getTexture={}, replacedFaces={}",
        stats.getTextureCalls,
        stats.replacedFaces);

    mHook.uninstall();
    return true;
}

bool ContinuityMod::unload() {
    mConfigFile.reset();
    return true;
}

}
