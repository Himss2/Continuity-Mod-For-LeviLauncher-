#include "mod/ContinuityMod.h"
#include "engine/Signatures.h"
#include "util/Properties.h"
#include <filesystem>
#include <fmt/format.h>

namespace continuity_bedrock {
ContinuityMod& ContinuityMod::instance() { static ContinuityMod i; return i; }
ContinuityMod::ContinuityMod() : mSelf(*ll::mod::NativeMod::current()) {}

bool ContinuityMod::load() {
    auto& log = getSelf().getLogger();
    std::error_code ec; std::filesystem::create_directories(getSelf().getConfigDir(), ec);
    mConfigFile.emplace();
    if (!mConfigFile->load()) { log.error("Failed to load config"); return false; }
    mConfig = mConfigFile->value();
    const auto rulePath = getSelf().getResourceDir() / "continuity" / "rules" / "glass.properties";
    const auto props = util::loadProperties(rulePath);
    log.info("Loaded Continuity Bedrock POC; rule={} method={} connect={}", rulePath.string(),
             props.contains("method") ? props.at("method") : "?",
             props.contains("connect") ? props.at("connect") : "?");
    return true;
}

bool ContinuityMod::enable() {
    auto& log = getSelf().getLogger();
    if (!mConfig.enabled) { log.info("Disabled by config"); return true; }
    mAddresses = engine::resolveAddresses();
    log.info("RE 1.26.52.3: _getTexture={:#x}, cacheGetBlock={:#x}, atlasUv={:#x}, useNew={:#x}",
             mAddresses.blockTessellatorGetTexture, mAddresses.blockTessellatorCacheGetBlock,
             mAddresses.blockGraphicsGetTextureUv, mAddresses.useNewTessellation);
    if (!engine::coreAddressesReady(mAddresses)) {
        log.error("Core renderer signatures did not resolve. Hooks are NOT installed.");
        return true; // fail-safe: unsupported build must not crash Minecraft
    }
    const bool ok = mHook.install(mAddresses, mConfig, [&](const std::string& s) { getSelf().getLogger().info("{}", s); });
    if (!ok) log.error("Failed to install legacy _getTexture hook");
    else log.info("Renderer diagnostic hook installed. Legacy CTM replacement={}", mConfig.enableLegacyCtm);
    return true;
}

bool ContinuityMod::disable() {
    const auto s = mHook.stats();
    getSelf().getLogger().info("Renderer stats: getTexture={}, glassLegacy={}, pipelineChecks={}, newTrue={}, newFalse={}",
                               s.getTextureCalls, s.glassLegacyHits, s.pipelineChecks, s.pipelineTrue, s.pipelineFalse);
    mHook.uninstall();
    return true;
}
bool ContinuityMod::unload() { mConfigFile.reset(); return true; }
}
