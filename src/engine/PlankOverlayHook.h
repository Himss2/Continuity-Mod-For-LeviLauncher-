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

    bool emitNeighborOverlay(
        uint8_t face,
        uint8_t localEdge,
        void* blockTessellator,
        void* tessellator,
        const void* targetBlock,
        const BlockPos& targetPos,
        const Vec3Raw& renderPos,
        const AabbRaw& originalShape
    );

    void emitJaggedEdge(
        uint8_t face,
        uint8_t localEdge,
        FaceFn original,
        void* blockTessellator,
        void* tessellator,
        const void* targetBlock,
        const Vec3Raw& renderPos,
        const TextureUvSet& sourceTexture,
        const AabbRaw& originalShape
    );

    static std::string_view blockFullName(const void* block);
    static bool isPlank(const void* block);
    static bool isGlassLike(const void* block);
    static bool isAirLike(const void* block);
    static bool isFullCube(const AabbRaw& shape);
    static BlockPos neighborPos(
        const BlockPos& pos,
        uint8_t face,
        uint8_t localEdge
    );
    static BlockPos faceNormal(uint8_t face);
    static AabbRaw makeStrip(
        const AabbRaw& original,
        uint8_t face,
        float u0,
        float u1,
        float v0,
        float v1
    );
    static void offsetFace(AabbRaw& shape, uint8_t face, float epsilon);

    static PlankOverlayHook* sInstance;

    Addresses mAddresses{};
    LegacyCtmHook* mTextureHook{};
    CacheGetBlockFn mGetBlock{};
    std::array<FaceHook, 4> mFaces{};
    LogFn mLog;
    std::atomic_bool mEnabled{true};
    std::atomic_bool mLoggedFirstOverlay{false};
};

}
