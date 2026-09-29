#include "mod/ContinuityMod.h"

#include "engine/Signatures.h"

#include <filesystem>
#include <fmt/format.h>
#include <pl/ModMenu.hpp>

namespace continuity_bedrock {

namespace {
constexpr std::string_view kModuleId = "continuity_bedrock.connected_textures";
}

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

    {
        std::lock_guard lock(mConfigMutex);
        mConfigFile.emplace();

        if (!mConfigFile->load()) {
            log.error("Failed to load config");
            mConfigFile.reset();
            return false;
        }

        mConfig = mConfigFile->value();
    }

    const auto rulesDir = getSelf().getResourceDir() / "continuity" / "rules";
    const bool loaded = mRules.load(
        rulesDir,
        [&](const std::string& message) {
            getSelf().getLogger().info("{}", message);
        });

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
        "RE 1.26.52.3: _getTexture={:#x}, cacheGetBlock={:#x}, atlasUv={:#x}, "
        "faces=[N:{:#x},S:{:#x},W:{:#x},E:{:#x}]",
        mAddresses.blockTessellatorGetTexture,
        mAddresses.blockTessellatorCacheGetBlock,
        mAddresses.blockGraphicsGetTextureUv,
        mAddresses.tessellateFaceNorth,
        mAddresses.tessellateFaceSouth,
        mAddresses.tessellateFaceWest,
        mAddresses.tessellateFaceEast);

    if (!engine::coreAddressesReady(mAddresses)) {
        log.error("Core renderer signatures did not resolve. Hooks are NOT installed.");
        return true;
    }

    const bool hookOk = mHook.install(
        mAddresses,
        mConfig,
        &mRules,
        [&](const std::string& message) {
            getSelf().getLogger().info("{}", message);
        });

    if (!hookOk) {
        log.error("Failed to install generic Continuity legacy hook");
        return true;
    }

    const bool overlayOk = mPlankOverlay.install(
        mAddresses,
        &mHook,
        mHook.ruleEngineEnabled(),
        [&](const std::string& message) {
            getSelf().getLogger().info("{}", message);
        });

    if (!overlayOk) {
        log.warn(
            "Plank overlay POC hooks are unavailable; base Continuity rules remain active");
    }

    if (!registerModMenu()) {
        log.warn("Failed to register Continuity in Levi Mod Menu");
    }

    log.info(
        "Generic rule hook active; rules={}, ruleEngine={}, plankOverlayPOC={}, modMenu={}",
        mRules.ruleCount(),
        mHook.ruleEngineEnabled(),
        overlayOk,
        mMenuRegistered);

    return true;
}

bool ContinuityMod::disable() {
    unregisterModMenu();

    const auto stats = mHook.stats();
    getSelf().getLogger().info(
        "Renderer stats: getTexture={}, replacedFaces={}",
        stats.getTextureCalls,
        stats.replacedFaces);

    mPlankOverlay.uninstall();
    mHook.uninstall();
    return true;
}

bool ContinuityMod::unload() {
    unregisterModMenu();

    std::lock_guard lock(mConfigMutex);
    mConfigFile.reset();
    return true;
}

bool ContinuityMod::registerModMenu() {
    if (mMenuRegistered) return true;

    mMenuRegistered =
        pl::modmenu::ModuleBuilder(
            std::string(kModuleId),
            "Continuity Connected Textures")
            .modId(getSelf().getId())
            .description(
                "Toggle Continuity connected-texture rules and the plank overlay POC. "
                "Glass remains delegated to BedrockTools.")
            .defaultEnabled(mHook.ruleEngineEnabled())
            .hideInHudEditor(true)
            .onToggle(&ContinuityMod::onModMenuToggle)
            .registerModule();

    if (mMenuRegistered) {
        getSelf().getLogger().info(
            "Registered Levi Mod Menu module: {}",
            kModuleId);
    }

    return mMenuRegistered;
}

void ContinuityMod::unregisterModMenu() {
    if (!mMenuRegistered) return;

    pl::modmenu::unregisterModule(kModuleId);
    mMenuRegistered = false;
}

void ContinuityMod::onModMenuToggle(
    std::string_view moduleId,
    bool enabled
) {
    instance().handleModMenuToggle(moduleId, enabled);
}

void ContinuityMod::handleModMenuToggle(
    std::string_view moduleId,
    bool enabled
) {
    if (moduleId != kModuleId) return;

    mHook.setRuleEngineEnabled(enabled);
    mPlankOverlay.setEnabled(enabled);
    persistRuleEngineEnabled(enabled);

    getSelf().getLogger().info(
        "Mod Menu: Continuity connected textures {}. "
        "Automatic chunk rebuild is disabled for stability; existing meshes "
        "refresh on the next natural chunk rebuild.",
        enabled ? "ON" : "OFF");
}

bool ContinuityMod::persistRuleEngineEnabled(bool enabled) {
    std::lock_guard lock(mConfigMutex);

    mConfig.enableRuleEngine = enabled;

    if (!mConfigFile) return false;

    mConfigFile->value().enableRuleEngine = enabled;
    const bool saved = mConfigFile->save();

    if (!saved) {
        getSelf().getLogger().warn(
            "Failed to persist enableRuleEngine={} after Mod Menu toggle",
            enabled);
    }

    return saved;
}

}
