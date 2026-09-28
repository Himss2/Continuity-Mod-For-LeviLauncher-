#include "mod/ContinuityMod.h"
#include "engine/Signatures.h"
#include "util/Properties.h"
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

    const auto rulePath =
        getSelf().getResourceDir() / "continuity" / "rules" / "bookshelf.properties";
    const auto props = util::loadProperties(rulePath);

    log.info(
        "Loaded Continuity Bedrock 0.1.3 POC; rule={} method={} connect={}; glass excluded",
        rulePath.string(),
        props.contains("method") ? props.at("method") : "?",
        props.contains("connect") ? props.at("connect") : "?");

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
        [&](const std::string& s) { getSelf().getLogger().info("{}", s); });

    if (!ok) {
        log.error("Failed to install legacy _getTexture hook");
    } else {
        log.info(
            "Bookshelf horizontal POC installed={}, useNewTessellation diagnostic disabled",
            mConfig.enableBookshelfPoc);
    }

    return true;
}

bool ContinuityMod::disable() {
    const auto s = mHook.stats();
    getSelf().getLogger().info(
        "Renderer stats: getTexture={}, bookshelfFaces={}, atlasProbeAttempts={}",
        s.getTextureCalls,
        s.bookshelfFaces,
        s.atlasProbeAttempts);

    mHook.uninstall();
    return true;
}

bool ContinuityMod::unload() {
    mConfigFile.reset();
    return true;
}

}
