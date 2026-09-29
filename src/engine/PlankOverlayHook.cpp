#include "engine/PlankOverlayHook.h"

#include "engine/CtmResolver.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <pl/memory/Hook.hpp>
#include <utility>

namespace continuity_bedrock::engine {

PlankOverlayHook* PlankOverlayHook::sInstance = nullptr;

namespace {

constexpr std::ptrdiff_t kBlockTessellatorCacheOffset = 0x678;
constexpr std::ptrdiff_t kCurrentShapeAabbOffset = 0x5F0;

constexpr std::ptrdiff_t kBlockTypeOffset = 0x68;
constexpr std::ptrdiff_t kBlockTypeNameInfoOffset = 0x88;
constexpr std::ptrdiff_t kNameInfoFullNameOffset = 0x40;
constexpr std::ptrdiff_t kHashedStringStringOffset = 0x8;

constexpr std::ptrdiff_t kTextureU0 = 0x4;
constexpr std::ptrdiff_t kTextureV0 = 0x8;
constexpr std::ptrdiff_t kTextureU1 = 0xC;
constexpr std::ptrdiff_t kTextureV1 = 0x10;
constexpr std::ptrdiff_t kTextureIsotropic = 0x50;
constexpr std::size_t kTextureUvSize = 0x58;
constexpr std::size_t kTextureUvAlignment = 0x8;

// Four 4-pixel-high teeth. Single-sided overlays may reach farther into the
// target, while dual-sided overlays are clamped below half width so the two
// source textures never overlap.
constexpr std::array<float, 4> kDepth = {
    8.0f / 16.0f,
    5.0f / 16.0f,
    10.0f / 16.0f,
    6.0f / 16.0f,
};

constexpr std::array<float, 5> kBand = {
    0.0f,
    4.0f / 16.0f,
    8.0f / 16.0f,
    12.0f / 16.0f,
    1.0f,
};

constexpr float kDualSideMaxDepth = 7.0f / 16.0f;
constexpr float kMinRegionWidth = 1.0f / 1024.0f;

constexpr uint8_t kClassPlank = 1u << 0;
constexpr uint8_t kClassGlass = 1u << 1;
constexpr uint8_t kClassAir   = 1u << 2;

struct ClassCacheEntry {
    const void* block{};
    uint8_t flags{};
};

thread_local std::array<ClassCacheEntry, 64> gClassCache{};

struct HorizontalNeighborCache {
    void* blockTessellator{};
    const void* centerBlock{};
    BlockPos centerPos{};
    bool valid{};
    // -X, +X, -Z, +Z
    std::array<const void*, 4> neighbors{};
};

thread_local HorizontalNeighborCache gHorizontalCache{};

template <typename T>
T& field(void* base, std::ptrdiff_t offset) {
    return *reinterpret_cast<T*>(
        reinterpret_cast<std::uintptr_t>(base) + offset);
}

bool samePos(const BlockPos& a, const BlockPos& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

size_t horizontalIndex(int dx, int dz) {
    if (dx < 0) return 0;
    if (dx > 0) return 1;
    if (dz < 0) return 2;
    return 3;
}

class TextureClone final {
public:
    using CopyCtorFn = void* (*)(void*, const void*);
    using DtorFn = void (*)(void*);

    TextureClone(
        const void* source,
        CopyCtorFn copyCtor,
        DtorFn dtor
    )
        : mDtor(dtor) {
        if (!source || !copyCtor || !dtor) return;

        copyCtor(mStorage.data(), source);
        mConstructed = true;

    }

    ~TextureClone() {
        if (mConstructed && mDtor) {
            mDtor(mStorage.data());
        }
    }

    TextureClone(const TextureClone&) = delete;
    TextureClone& operator=(const TextureClone&) = delete;
    TextureClone(TextureClone&&) = delete;
    TextureClone& operator=(TextureClone&&) = delete;

    bool valid() const {
        return mConstructed;
    }

    TextureUvSet* texture() {
        if (!mConstructed) return nullptr;

        // Geometry subdivision already makes BlockTessellator select the
        // corresponding portion of the source texture. Do not crop UV a
        // second time here. Only disable isotropic random rotation so the
        // plank rows stay aligned across the split regions.
        field<uint8_t>(mStorage.data(), kTextureIsotropic) = 0;
        return reinterpret_cast<TextureUvSet*>(mStorage.data());
    }

private:
    alignas(kTextureUvAlignment)
        std::array<std::byte, kTextureUvSize> mStorage{};

    DtorFn mDtor{};
    bool mConstructed{};
};

}

bool PlankOverlayHook::install(
    const Addresses& addresses,
    LegacyCtmHook* textureHook,
    bool enabled,
    LogFn log
) {
    uninstall();

    mAddresses = addresses;
    mTextureHook = textureHook;
    mGetBlock =
        reinterpret_cast<CacheGetBlockFn>(addresses.blockTessellatorCacheGetBlock);
    mTextureCopyCtor =
        reinterpret_cast<TextureUvCopyCtorFn>(addresses.textureUvCopyCtor);
    mTextureDtor =
        reinterpret_cast<TextureUvDtorFn>(addresses.textureUvDtor);

    mLog = std::move(log);
    mEnabled.store(enabled, std::memory_order_relaxed);
    mLoggedFirstOverlay.store(false, std::memory_order_relaxed);

    if (!mTextureHook
        || !mGetBlock
        || !mTextureCopyCtor
        || !mTextureDtor) {
        if (mLog) {
            mLog(
                "Plank overlay POC unavailable: texture lifecycle or renderer "
                "dependencies did not resolve");
        }
        return false;
    }

    const std::array<uintptr_t, 4> addressesByFace = {
        addresses.tessellateFaceNorth,
        addresses.tessellateFaceSouth,
        addresses.tessellateFaceWest,
        addresses.tessellateFaceEast,
    };

    const std::array<FaceFn, 4> detours = {
        &PlankOverlayHook::northDetour,
        &PlankOverlayHook::southDetour,
        &PlankOverlayHook::westDetour,
        &PlankOverlayHook::eastDetour,
    };

    sInstance = this;

    bool any = false;
    for (size_t i = 0; i < mFaces.size(); ++i) {
        auto& hook = mFaces[i];
        hook.address = addressesByFace[i];

        if (!hook.address) continue;

        hook.installed = pl::memory::hook(
            reinterpret_cast<void*>(hook.address),
            reinterpret_cast<void*>(detours[i]),
            reinterpret_cast<void**>(&hook.original)) == 0;

        any |= hook.installed;
    }

    if (mLog) {
        mLog(
            "Plank overlay POC hooks: north="
            + std::string(mFaces[0].installed ? "ok" : "fail")
            + ", south=" + (mFaces[1].installed ? "ok" : "fail")
            + ", west=" + (mFaces[2].installed ? "ok" : "fail")
            + ", east=" + (mFaces[3].installed ? "ok" : "fail")
            + "; coplanar non-overlap renderer=true; safeUvCopy=true");
    }

    if (!any && sInstance == this) {
        sInstance = nullptr;
    }

    return any;
}

void PlankOverlayHook::uninstall() {
    const std::array<FaceFn, 4> detours = {
        &PlankOverlayHook::northDetour,
        &PlankOverlayHook::southDetour,
        &PlankOverlayHook::westDetour,
        &PlankOverlayHook::eastDetour,
    };

    for (size_t i = 0; i < mFaces.size(); ++i) {
        auto& hook = mFaces[i];

        if (hook.installed && hook.address) {
            pl::memory::unhook(
                reinterpret_cast<void*>(hook.address),
                reinterpret_cast<void*>(detours[i]));
        }

        hook = {};
    }

    mTextureHook = nullptr;
    mGetBlock = nullptr;
    mTextureCopyCtor = nullptr;
    mTextureDtor = nullptr;

    if (sInstance == this) {
        sInstance = nullptr;
    }
}

void PlankOverlayHook::northDetour(
    void* a0,
    void* a1,
    const void* a2,
    const Vec3Raw* a3,
    const TextureUvSet* a4
) {
    if (sInstance) sInstance->onFace(2, a0, a1, a2, a3, a4);
}

void PlankOverlayHook::southDetour(
    void* a0,
    void* a1,
    const void* a2,
    const Vec3Raw* a3,
    const TextureUvSet* a4
) {
    if (sInstance) sInstance->onFace(3, a0, a1, a2, a3, a4);
}

void PlankOverlayHook::westDetour(
    void* a0,
    void* a1,
    const void* a2,
    const Vec3Raw* a3,
    const TextureUvSet* a4
) {
    if (sInstance) sInstance->onFace(4, a0, a1, a2, a3, a4);
}

void PlankOverlayHook::eastDetour(
    void* a0,
    void* a1,
    const void* a2,
    const Vec3Raw* a3,
    const TextureUvSet* a4
) {
    if (sInstance) sInstance->onFace(5, a0, a1, a2, a3, a4);
}

void PlankOverlayHook::onFace(
    uint8_t face,
    void* blockTessellator,
    void* tessellator,
    const void* block,
    const Vec3Raw* position,
    const TextureUvSet* inputTexture
) {
    const size_t index = static_cast<size_t>(face - 2);
    FaceFn original = index < mFaces.size() ? mFaces[index].original : nullptr;
    if (!original) return;

    const auto vanilla = [&]() {
        original(
            blockTessellator,
            tessellator,
            block,
            position,
            inputTexture);
    };

    if (!mEnabled.load(std::memory_order_relaxed)
        || !blockTessellator
        || !tessellator
        || !block
        || !position
        || !inputTexture
        || isPlank(block)
        || isGlassLike(block)
        || !mTextureCopyCtor
        || !mTextureDtor) {
        vanilla();
        return;
    }

    auto* shape = reinterpret_cast<AabbRaw*>(
        reinterpret_cast<uintptr_t>(blockTessellator)
        + kCurrentShapeAabbOffset);

    const AabbRaw originalShape = *shape;
    if (!isFullCube(originalShape)) {
        vanilla();
        return;
    }

    const BlockPos targetPos{
        static_cast<int32_t>(std::floor(position->x)),
        static_cast<int32_t>(std::floor(position->y)),
        static_cast<int32_t>(std::floor(position->z)),
    };

    auto* blockCache = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(blockTessellator)
        + kBlockTessellatorCacheOffset);

    const void* leftSource = horizontalNeighbor(
        blockCache,
        blockTessellator,
        block,
        targetPos,
        face,
        0);

    const void* rightSource = horizontalNeighbor(
        blockCache,
        blockTessellator,
        block,
        targetPos,
        face,
        2);

    bool leftActive =
        leftSource
        && isPlank(leftSource)
        && !sourceSurroundedOnFourSides(
            blockCache,
            neighborPos(targetPos, face, 0),
            face);

    bool rightActive =
        rightSource
        && isPlank(rightSource)
        && !sourceSurroundedOnFourSides(
            blockCache,
            neighborPos(targetPos, face, 2),
            face);

    if (!leftActive && !rightActive) {
        vanilla();
        return;
    }

    TextureClone baseTexture(
        inputTexture,
        mTextureCopyCtor,
        mTextureDtor);

    if (!baseTexture.valid()) {
        vanilla();
        return;
    }

    std::optional<TextureClone> leftTexture;
    std::optional<TextureClone> rightTexture;

    BlockPos leftPos{};
    BlockPos rightPos{};

    if (leftActive) {
        leftPos = neighborPos(targetPos, face, 0);
        const TextureUvSet* source = mTextureHook->queryOriginalTexture(
            blockTessellator,
            &leftPos,
            leftSource,
            face,
            0,
            nullptr);

        if (source) {
            leftTexture.emplace(
                source,
                mTextureCopyCtor,
                mTextureDtor);
        }

        leftActive = leftTexture && leftTexture->valid();
    }

    if (rightActive) {
        rightPos = neighborPos(targetPos, face, 2);
        const TextureUvSet* source = mTextureHook->queryOriginalTexture(
            blockTessellator,
            &rightPos,
            rightSource,
            face,
            0,
            nullptr);

        if (source) {
            rightTexture.emplace(
                source,
                mTextureCopyCtor,
                mTextureDtor);
        }

        rightActive = rightTexture && rightTexture->valid();
    }

    if (!leftActive && !rightActive) {
        vanilla();
        return;
    }

    const bool dualSided = leftActive && rightActive;

    const auto renderRegion = [&](
        TextureClone& texture,
        float u0,
        float u1,
        float v0,
        float v1
    ) {
        if (u1 - u0 <= kMinRegionWidth
            || v1 - v0 <= kMinRegionWidth) {
            return;
        }

        TextureUvSet* safeTexture = texture.texture();
        if (!safeTexture) return;

        *shape = makeRegion(
            originalShape,
            face,
            u0,
            u1,
            v0,
            v1);

        original(
            blockTessellator,
            tessellator,
            block,
            position,
            safeTexture);
    };

    // No face is drawn twice. Each 4px-high band is partitioned into:
    // [left plank] [original target] [right plank].
    // All three regions stay on the exact vanilla face plane, so there is
    // neither depth fighting nor a visible side extrusion.
    for (size_t band = 0; band < 4; ++band) {
        float leftDepth = leftActive ? kDepth[band] : 0.0f;
        float rightDepth = rightActive ? kDepth[band] : 0.0f;

        if (dualSided) {
            leftDepth = std::min(leftDepth, kDualSideMaxDepth);
            rightDepth = std::min(rightDepth, kDualSideMaxDepth);
        }

        const float v0 = kBand[band];
        const float v1 = kBand[band + 1];

        if (leftActive) {
            renderRegion(
                *leftTexture,
                0.0f,
                leftDepth,
                v0,
                v1);
        }

        const float centerStart = leftDepth;
        const float centerEnd = 1.0f - rightDepth;

        if (centerEnd > centerStart + kMinRegionWidth) {
            renderRegion(
                baseTexture,
                centerStart,
                centerEnd,
                v0,
                v1);
        }

        if (rightActive) {
            renderRegion(
                *rightTexture,
                1.0f - rightDepth,
                1.0f,
                v0,
                v1);
        }
    }

    *shape = originalShape;

    if (!mLoggedFirstOverlay.exchange(true, std::memory_order_relaxed)
        && mLog) {
        mLog(
            "PLANK OVERLAY HIT: target="
            + std::string(blockFullName(block))
            + ", left=" + (leftActive
                ? std::string(blockFullName(leftSource))
                : "none")
            + ", right=" + (rightActive
                ? std::string(blockFullName(rightSource))
                : "none")
            + ", face=" + std::to_string(face)
            + ", coplanarSplit=true, uvCrop=geometry-only");
    }
}

bool PlankOverlayHook::sourceSurroundedOnFourSides(
    void* blockCache,
    const BlockPos& sourcePos,
    uint8_t face
) const {
    if (!blockCache || !mGetBlock) return false;

    const auto dirs = ctm::directionsForFace(face);

    for (const auto& d : dirs) {
        const BlockPos p{
            sourcePos.x + d.x,
            sourcePos.y + d.y,
            sourcePos.z + d.z,
        };

        const void* neighbor = mGetBlock(blockCache, &p);

        // Suppress only a plank boxed by four NON-plank blocks.
        // A neighboring plank means it is part of a plank row/wall.
        if (!neighbor || isAirLike(neighbor) || isPlank(neighbor)) {
            return false;
        }
    }

    return true;
}

const void* PlankOverlayHook::horizontalNeighbor(
    void* blockCache,
    void* blockTessellator,
    const void* centerBlock,
    const BlockPos& centerPos,
    uint8_t face,
    uint8_t localEdge
) const {
    if (!blockCache || !mGetBlock) return nullptr;

    if (!gHorizontalCache.valid
        || gHorizontalCache.blockTessellator != blockTessellator
        || gHorizontalCache.centerBlock != centerBlock
        || !samePos(gHorizontalCache.centerPos, centerPos)) {
        gHorizontalCache.blockTessellator = blockTessellator;
        gHorizontalCache.centerBlock = centerBlock;
        gHorizontalCache.centerPos = centerPos;
        gHorizontalCache.valid = true;

        const std::array<BlockPos, 4> positions = {{
            {centerPos.x - 1, centerPos.y, centerPos.z},
            {centerPos.x + 1, centerPos.y, centerPos.z},
            {centerPos.x, centerPos.y, centerPos.z - 1},
            {centerPos.x, centerPos.y, centerPos.z + 1},
        }};

        for (size_t i = 0; i < positions.size(); ++i) {
            gHorizontalCache.neighbors[i] =
                mGetBlock(blockCache, &positions[i]);
        }
    }

    const auto dirs = ctm::directionsForFace(face);
    const auto d = dirs[localEdge & 3u];

    if (d.y != 0) return nullptr;

    return gHorizontalCache.neighbors[
        horizontalIndex(d.x, d.z)];
}

std::string_view PlankOverlayHook::blockFullName(const void* block) {
    if (!block) return {};

    const void* type = *reinterpret_cast<void* const*>(
        reinterpret_cast<uintptr_t>(block) + kBlockTypeOffset);
    if (!type) return {};

    const uintptr_t stringAddress =
        reinterpret_cast<uintptr_t>(type)
        + kBlockTypeNameInfoOffset
        + kNameInfoFullNameOffset
        + kHashedStringStringOffset;

    const auto* name = reinterpret_cast<const std::string*>(stringAddress);
    if (!name
        || name->empty()
        || name->size() > 256
        || name->data() == nullptr) {
        return {};
    }

    return {name->data(), name->size()};
}

uint8_t PlankOverlayHook::blockClassFlags(const void* block) {
    if (!block) return kClassAir;

    const uintptr_t key = reinterpret_cast<uintptr_t>(block);
    auto& entry = gClassCache[(key >> 4u) & (gClassCache.size() - 1u)];

    if (entry.block == block) {
        return entry.flags;
    }

    std::string_view name = blockFullName(block);
    constexpr std::string_view prefix = "minecraft:";
    if (name.starts_with(prefix)) {
        name.remove_prefix(prefix.size());
    }

    uint8_t flags = 0;

    if (name.ends_with("_planks")) {
        flags |= kClassPlank;
    }

    if (name == "glass"
        || name == "glass_pane"
        || name == "tinted_glass"
        || name.ends_with("_stained_glass")
        || name.ends_with("_stained_glass_pane")) {
        flags |= kClassGlass;
    }

    if (name == "air"
        || name == "cave_air"
        || name == "void_air") {
        flags |= kClassAir;
    }

    entry.block = block;
    entry.flags = flags;
    return flags;
}

bool PlankOverlayHook::isPlank(const void* block) {
    return (blockClassFlags(block) & kClassPlank) != 0;
}

bool PlankOverlayHook::isGlassLike(const void* block) {
    return (blockClassFlags(block) & kClassGlass) != 0;
}

bool PlankOverlayHook::isAirLike(const void* block) {
    return !block || (blockClassFlags(block) & kClassAir) != 0;
}

bool PlankOverlayHook::isFullCube(const AabbRaw& shape) {
    const float dx = shape.maxX - shape.minX;
    const float dy = shape.maxY - shape.minY;
    const float dz = shape.maxZ - shape.minZ;

    return dx > 0.99f
        && dy > 0.99f
        && dz > 0.99f
        && dx < 1.01f
        && dy < 1.01f
        && dz < 1.01f;
}

BlockPos PlankOverlayHook::neighborPos(
    const BlockPos& pos,
    uint8_t face,
    uint8_t localEdge
) {
    const auto dirs = ctm::directionsForFace(face);
    const auto d = dirs[localEdge & 3u];

    return {
        pos.x + d.x,
        pos.y + d.y,
        pos.z + d.z,
    };
}

PlankOverlayHook::AabbRaw PlankOverlayHook::makeRegion(
    const AabbRaw& original,
    uint8_t face,
    float u0,
    float u1,
    float v0,
    float v1
) {
    AabbRaw out = original;

    u0 = std::clamp(u0, 0.0f, 1.0f);
    u1 = std::clamp(u1, 0.0f, 1.0f);
    v0 = std::clamp(v0, 0.0f, 1.0f);
    v1 = std::clamp(v1, 0.0f, 1.0f);

    out.minY = lerp(original.minY, original.maxY, v0);
    out.maxY = lerp(original.minY, original.maxY, v1);

    switch (face) {
    case 2: // north: texture-left=+X, texture-right=-X
        out.minX = lerp(original.maxX, original.minX, u1);
        out.maxX = lerp(original.maxX, original.minX, u0);
        break;

    case 3: // south: texture-left=-X, texture-right=+X
        out.minX = lerp(original.minX, original.maxX, u0);
        out.maxX = lerp(original.minX, original.maxX, u1);
        break;

    case 4: // west: texture-left=-Z, texture-right=+Z
        out.minZ = lerp(original.minZ, original.maxZ, u0);
        out.maxZ = lerp(original.minZ, original.maxZ, u1);
        break;

    case 5: // east: texture-left=+Z, texture-right=-Z
        out.minZ = lerp(original.maxZ, original.minZ, u1);
        out.maxZ = lerp(original.maxZ, original.minZ, u0);
        break;

    default:
        break;
    }

    return out;
}

}
