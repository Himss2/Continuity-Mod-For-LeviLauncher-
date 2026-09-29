#pragma once

#include "engine/LegacyCtmHook.h"
#include "engine/Signatures.h"
#include "engine/Types.h"

#include <array>
#include <atomic>
#include <functional>
#include <string>
#include <string_view>

namespace continuity_bedrock::engine {

class PlankOverlayHook {
public:
    using LogFn = std::function<void(const std::string&)>;

    bool install(
        const Addresses& addresses,
        LegacyCtmHook* textureHook,
        bool enabled,
        LogFn log
    );
    void uninstall();

    void setEnabled(bool enabled) {
        mEnabled.store(enabled, std::memory_order_release);
    }

    bool enabled() const {
        return mEnabled.load(std::memory_order_acquire);
    }

private:
    struct Vec3Raw {
        float x, y, z;
    };

    struct AabbRaw {
        float minX, minY, minZ;
        float maxX, maxY, maxZ;
    };
    static_assert(sizeof(AabbRaw) == 24);

    using FaceFn =
        void (*)(void*, void*, const void*, const Vec3Raw*, const TextureUvSet*);
    using CacheGetBlockFn = const void* (*)(void*, const BlockPos*);
    using TextureUvCopyCtorFn = void* (*)(void*, const void*);
    using TextureUvDtorFn = void (*)(void*);

    struct FaceHook {
        uintptr_t address{};
        FaceFn original{};
        bool installed{};
    };

    static void northDetour(
        void*, void*, const void*, const Vec3Raw*, const TextureUvSet*);
    static void southDetour(
        void*, void*, const void*, const Vec3Raw*, const TextureUvSet*);
    static void westDetour(
        void*, void*, const void*, const Vec3Raw*, const TextureUvSet*);
    static void eastDetour(
        void*, void*, const void*, const Vec3Raw*, const TextureUvSet*);

    void onFace(
        uint8_t face,
        void* blockTessellator,
        void* tessellator,
        const void* block,
        const Vec3Raw* position,
        const TextureUvSet* inputTexture
    );

    bool sourceSurroundedOnFourSides(
        void* blockCache,
        const BlockPos& sourcePos,
        uint8_t face
    ) const;

    const void* horizontalNeighbor(
        void* blockCache,
        void* blockTessellator,
        const void* centerBlock,
        const BlockPos& centerPos,
        uint8_t face,
        uint8_t localEdge
    ) const;

    static std::string_view blockFullName(const void* block);
    static uint8_t blockClassFlags(const void* block);
    static bool isPlank(const void* block);
    static bool isGlassLike(const void* block);
    static bool isAirLike(const void* block);
    static bool isFullCube(const AabbRaw& shape);
    static BlockPos neighborPos(
        const BlockPos& pos,
        uint8_t face,
        uint8_t localEdge
    );
    static AabbRaw makeRegion(
        const AabbRaw& original,
        uint8_t face,
        float u0,
        float u1,
        float v0,
        float v1
    );

    static PlankOverlayHook* sInstance;

    Addresses mAddresses{};
    LegacyCtmHook* mTextureHook{};
    CacheGetBlockFn mGetBlock{};
    TextureUvCopyCtorFn mTextureCopyCtor{};
    TextureUvDtorFn mTextureDtor{};
    std::array<FaceHook, 4> mFaces{};
    LogFn mLog;
    std::atomic_bool mEnabled{true};
    std::atomic_bool mLoggedFirstOverlay{false};
};

}
