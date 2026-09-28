#pragma once

#include "engine/LegacyCtmHook.h"
#include "engine/RuleEngine.h"
#include "mod/Config.h"

#include <mutex>
#include <optional>
#include <string_view>

#include <pl/Mod.hpp>

namespace continuity_bedrock {

class ContinuityMod {
public:
    static ContinuityMod& instance();
    ContinuityMod();

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
    bool persistRuleEngineEnabled(bool enabled);

    ll::mod::NativeMod& mSelf;
    ModConfig mConfig{};
    std::mutex mConfigMutex;
    std::optional<pl::config::ConfigFile<ModConfig>> mConfigFile;

    engine::Addresses mAddresses{};
    engine::RuleEngine mRules;
    engine::LegacyCtmHook mHook;

    bool mMenuRegistered{};
};

}
