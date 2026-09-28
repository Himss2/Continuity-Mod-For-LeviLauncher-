#pragma once
#include "engine/LegacyCtmHook.h"
#include "mod/Config.h"
#include <optional>
#include <pl/Mod.hpp>

namespace continuity_bedrock {
class ContinuityMod {
public:
    static ContinuityMod& instance();
    ContinuityMod();
    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }
    bool load(); bool enable(); bool disable(); bool unload();
private:
    ll::mod::NativeMod& mSelf;
    ModConfig mConfig{};
    std::optional<pl::config::ConfigFile<ModConfig>> mConfigFile;
    engine::Addresses mAddresses{};
    engine::LegacyCtmHook mHook;
};
}
