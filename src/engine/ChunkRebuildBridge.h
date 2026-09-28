#pragma once

#include "engine/Signatures.h"

#include <atomic>
#include <functional>
#include <string>

namespace continuity_bedrock::engine {

class ChunkRebuildBridge {
public:
    using LogFn = std::function<void(const std::string&)>;

    bool install(const Addresses& addresses, LogFn log);
    void uninstall();

    void request();
    bool ready() const {
        return mInstalled && mSetAllDirty != nullptr;
    }

private:
    using ClientInstanceUpdateFn = void* (*)(void*, bool);
    using SetAllDirtyFn = void (*)(void*, bool, bool);

    static void* clientInstanceUpdateDetour(void*, bool);
    void* onClientInstanceUpdate(void*, bool);
    bool rebuild(void* clientInstance);

    static ChunkRebuildBridge* sInstance;

    Addresses mAddresses{};
    LogFn mLog;
    ClientInstanceUpdateFn mOriginalUpdate{};
    SetAllDirtyFn mSetAllDirty{};
    bool mInstalled{};
    std::atomic_bool mPending{false};
    std::atomic_bool mLoggedSuccess{false};
};

}
