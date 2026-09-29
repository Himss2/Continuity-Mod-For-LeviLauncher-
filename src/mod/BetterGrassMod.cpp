#include "mod/BetterGrassMod.h"

#include <fmt/format.h>
#include <pl/ModMenu.hpp>

#include <string>

namespace better_grass {

namespace {
constexpr std::string_view kModuleId = "better_grass.fancy";
}

BetterGrassMod& BetterGrassMod::instance() {
    static BetterGrassMod instance;
    return instance;
}

BetterGrassMod::BetterGrassMod()
    : mSelf(*ll::mod::NativeMod::current()) {}

bool BetterGrassMod::load() {
    auto& log = getSelf().getLogger();

    {
        std::lock_guard lock(mConfigMutex);
        mConfigFile.emplace();

        if (!mConfigFile->load()) {
            log.error("Failed to load Better Grass config");
            mConfigFile.reset();
            return false;
        }

        mConfig = mConfigFile->value();
    }

    log.info("Loaded Better Grass Fancy config; enabled={}", mConfig.enabled);
    return true;
}

bool BetterGrassMod::enable() {
    auto& log = getSelf().getLogger();

    mAddresses = engine::resolveAddresses();

    log.info(
        "RE 1.26.52.3: _getTexture={:#x}, cacheGetBlock={:#x}",
        mAddresses.blockTessellatorGetTexture,
        mAddresses.blockTessellatorCacheGetBlock);

    if (!engine::coreAddressesReady(mAddresses)) {
        log.error("Core renderer signatures did not resolve. Better Grass hook is NOT installed.");
        return true;
    }

    const bool hookOk = mHook.install(
        mAddresses,
        mConfig,
        [&](const std::string& message) {
            getSelf().getLogger().info("{}", message);
        });

    if (!hookOk) {
        log.error("Failed to install Better Grass renderer hook");
        return true;
    }

    if (!registerModMenu()) {
        log.warn("Failed to register Better Grass in Levi Mod Menu");
    }

    log.info(
        "Better Grass Fancy active; enabled={}, modMenu={}",
        mHook.enabled(),
        mMenuRegistered);

    return true;
}

bool BetterGrassMod::disable() {
    unregisterModMenu();

    const auto stats = mHook.stats();
    getSelf().getLogger().info(
        "Renderer stats: getTexture={}, replacedFaces={}",
        stats.getTextureCalls,
        stats.replacedFaces);

    mHook.uninstall();
    return true;
}

bool BetterGrassMod::unload() {
    unregisterModMenu();

    std::lock_guard lock(mConfigMutex);
    mConfigFile.reset();
    return true;
}

bool BetterGrassMod::registerModMenu() {
    if (mMenuRegistered) return true;

    mMenuRegistered =
        pl::modmenu::ModuleBuilder(
            std::string(kModuleId),
            "Better Grass")
            .modId(getSelf().getId())
            .description(
                "Fancy Better Grass: grass sides follow exposed terrain slopes. "
                "Textures come from Minecraft's active resource pack.")
            .defaultEnabled(mHook.enabled())
            .hideInHudEditor(true)
            .onToggle(&BetterGrassMod::onModMenuToggle)
            .registerModule();

    if (mMenuRegistered) {
        getSelf().getLogger().info(
            "Registered Levi Mod Menu module: {}",
            kModuleId);
    }

    return mMenuRegistered;
}

void BetterGrassMod::unregisterModMenu() {
    if (!mMenuRegistered) return;

    pl::modmenu::unregisterModule(kModuleId);
    mMenuRegistered = false;
}

void BetterGrassMod::onModMenuToggle(
    std::string_view moduleId,
    bool enabled
) {
    instance().handleModMenuToggle(moduleId, enabled);
}

void BetterGrassMod::handleModMenuToggle(
    std::string_view moduleId,
    bool enabled
) {
    if (moduleId != kModuleId) return;

    mHook.setEnabled(enabled);
    persistEnabled(enabled);

    getSelf().getLogger().info(
        "Mod Menu: Better Grass {}. Existing chunk meshes refresh on the "
        "next natural chunk rebuild.",
        enabled ? "ON" : "OFF");
}

bool BetterGrassMod::persistEnabled(bool enabled) {
    std::lock_guard lock(mConfigMutex);

    mConfig.enabled = enabled;

    if (!mConfigFile) return false;

    mConfigFile->value().enabled = enabled;
    const bool saved = mConfigFile->save();

    if (!saved) {
        getSelf().getLogger().warn(
            "Failed to persist enabled={} after Mod Menu toggle",
            enabled);
    }

    return saved;
}

}
