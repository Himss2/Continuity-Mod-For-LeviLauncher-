#pragma once

#include "engine/BetterGrassHook.h"
#include "engine/Signatures.h"
#include "mod/Config.h"

#include <mutex>
#include <optional>
#include <string_view>

#include <pl/Mod.hpp>

namespace better_grass {

class BetterGrassMod {
public:
    static BetterGrassMod& instance();
    BetterGrassMod();

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();
    bool unload();

private:
    static void onModMenuToggle(std::string_view moduleId, bool enabled);

    bool registerModMenu();
    void unregisterModMenu();
    void handleModMenuToggle(std::string_view moduleId, bool enabled);
    bool persistEnabled(bool enabled);

    ll::mod::NativeMod& mSelf;
    ModConfig mConfig{};
    std::mutex mConfigMutex;
    std::optional<pl::config::ConfigFile<ModConfig>> mConfigFile;

    engine::Addresses mAddresses{};
    engine::BetterGrassHook mHook;

    bool mMenuRegistered{};
};

}
