#include "engine/ChunkRebuildBridge.h"

#include <cstddef>
#include <cstdint>
#include <pl/memory/Hook.hpp>

namespace continuity_bedrock::engine {

ChunkRebuildBridge* ChunkRebuildBridge::sInstance = nullptr;

namespace {

// Verified against the same 1.26.52.3 layout used by BedrockTools.
constexpr std::size_t kClientInstanceLevelRenderer = 0x190;
constexpr std::size_t kLevelRendererChunkCoordinators = 0x28;
constexpr std::size_t kHashTableFirstNode = 0x10;
constexpr std::size_t kHashNodeNext = 0x0;
constexpr std::size_t kHashNodeValuePointer = 0x18;
constexpr std::size_t kMaxCoordinatorNodes = 64;

template <class T>
T& field(void* base, std::size_t offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(base) + offset);
}

}

bool ChunkRebuildBridge::install(
    const Addresses& addresses,
    LogFn log
) {
    uninstall();

    mAddresses = addresses;
    mLog = std::move(log);
    mPending.store(false, std::memory_order_relaxed);
    mLoggedSuccess.store(false, std::memory_order_relaxed);

    mSetAllDirty = reinterpret_cast<SetAllDirtyFn>(
        addresses.renderChunkCoordinatorSetAllDirty);

    if (!addresses.clientInstanceUpdate || !mSetAllDirty) {
        if (mLog) {
            mLog(
                "Chunk rebuild bridge unavailable: ClientInstance::update or "
                "RenderChunkCoordinator::setAllDirty did not resolve");
        }
        return false;
    }

    sInstance = this;

    mInstalled = pl::memory::hook(
        reinterpret_cast<void*>(addresses.clientInstanceUpdate),
        reinterpret_cast<void*>(&ChunkRebuildBridge::clientInstanceUpdateDetour),
        reinterpret_cast<void**>(&mOriginalUpdate)) == 0;

    if (!mInstalled) {
        mOriginalUpdate = nullptr;
        mSetAllDirty = nullptr;
        if (sInstance == this) sInstance = nullptr;

        if (mLog) mLog("Failed to install ClientInstance::update rebuild hook");
        return false;
    }

    if (mLog) {
        mLog("Chunk rebuild bridge installed for instant Mod Menu ON/OFF refresh");
    }
    return true;
}

void ChunkRebuildBridge::uninstall() {
    if (mInstalled && mAddresses.clientInstanceUpdate) {
        pl::memory::unhook(
            reinterpret_cast<void*>(mAddresses.clientInstanceUpdate),
            reinterpret_cast<void*>(&ChunkRebuildBridge::clientInstanceUpdateDetour));
    }

    mInstalled = false;
    mOriginalUpdate = nullptr;
    mSetAllDirty = nullptr;
    mPending.store(false, std::memory_order_relaxed);

    if (sInstance == this) sInstance = nullptr;
}

void ChunkRebuildBridge::request() {
    mPending.store(true, std::memory_order_release);
}

void* ChunkRebuildBridge::clientInstanceUpdateDetour(
    void* clientInstance,
    bool value
) {
    if (!sInstance) return nullptr;
    return sInstance->onClientInstanceUpdate(clientInstance, value);
}

void* ChunkRebuildBridge::onClientInstanceUpdate(
    void* clientInstance,
    bool value
) {
    void* result =
        mOriginalUpdate ? mOriginalUpdate(clientInstance, value) : nullptr;

    if (clientInstance
        && mPending.load(std::memory_order_acquire)
        && rebuild(clientInstance)) {
        mPending.store(false, std::memory_order_release);

        if (!mLoggedSuccess.exchange(true, std::memory_order_relaxed) && mLog) {
            mLog("Visible render chunks rebuilt after Continuity toggle");
        }
    }

    return result;
}

bool ChunkRebuildBridge::rebuild(void* clientInstance) {
    if (!clientInstance || !mSetAllDirty) return false;

    void* levelRenderer =
        field<void*>(clientInstance, kClientInstanceLevelRenderer);
    if (!levelRenderer) return false;

    void* node = field<void*>(
        levelRenderer,
        kLevelRendererChunkCoordinators + kHashTableFirstNode);

    bool rebuilt = false;
    std::size_t visited = 0;

    while (node && visited++ < kMaxCoordinatorNodes) {
        void* next = field<void*>(node, kHashNodeNext);
        void* coordinator = field<void*>(node, kHashNodeValuePointer);

        if (coordinator) {
            mSetAllDirty(coordinator, true, false);
            rebuilt = true;
        }

        node = next;
    }

    return rebuilt;
}

}
