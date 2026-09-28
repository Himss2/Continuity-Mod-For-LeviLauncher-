#pragma once
#include "engine/AtlasCache.h"
#include "engine/Signatures.h"
#include "engine/Types.h"
#include "mod/Config.h"
#include <atomic>
#include <functional>
#include <string>

namespace continuity_bedrock::engine {

struct HookStats {
    uint64_t getTextureCalls{};
    uint64_t bookshelfFaces{};
    uint64_t atlasProbeAttempts{};
};

class LegacyCtmHook {
public:
    using LogFn = std::function<void(const std::string&)>;

    bool install(const Addresses& a, const ModConfig& cfg, LogFn log);
    void uninstall();
    HookStats stats() const;

private:
    using GetTextureFn =
        const TextureUvSet* (*)(void*, const BlockPos*, const void*, uint8_t, int, const void*);
    using CacheGetBlockFn = const void* (*)(void*, const BlockPos*);

    static const TextureUvSet* getTextureDetour(
        void*, const BlockPos*, const void*, uint8_t, int, const void*);

    const TextureUvSet* onGetTexture(
        void*, const BlockPos*, const void*, uint8_t, int, const void*);
    void logDiagnostic(std::string message);

    static const void* blockType(const void* block);
    static bool hasFullName(const void* block, std::string_view expected);

    static LegacyCtmHook* sInstance;
    ModConfig mConfig{};
    LogFn mLog;
    Addresses mAddresses{};
    GetTextureFn mOriginalGetTexture{};
    CacheGetBlockFn mCacheGetBlock{};
    AtlasCache::GetTextureUvFn mGetTextureUv{};
    bool mGetTextureInstalled{};

    AtlasCache mAtlas;
    std::atomic<uintptr_t> mBookshelfType{};
    std::atomic<uint64_t> mGetTextureCalls{};
    std::atomic<uint64_t> mBookshelfFaces{};
    std::atomic<uint64_t> mAtlasProbeAttempts{};
    std::atomic<uint64_t> mPreAtlasCalls{};
    std::atomic<uint32_t> mDiagnosticLogCount{};
    std::atomic_bool mLoggedRuntimeHit{};
    std::atomic_bool mLoggedAtlasReady{};
    std::atomic_bool mLoggedBookshelf{};
};

}
