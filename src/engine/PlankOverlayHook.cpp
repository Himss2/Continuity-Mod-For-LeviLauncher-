#include "engine/PlankOverlayHook.h"

#include "engine/CtmResolver.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
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

constexpr float kFaceEpsilon = 0.0010f;

// Four deliberately irregular 16x-style depths, matching the visual language
// of Simple Overlays without baking any plank pixels into this mod.
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

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

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
    mLog = std::move(log);
    mEnabled.store(enabled, std::memory_order_relaxed);
    mLoggedFirstOverlay.store(false, std::memory_order_relaxed);

    if (!mTextureHook || !mGetBlock) {
        if (mLog) mLog("Plank overlay POC unavailable: renderer dependencies missing");
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
            "Plank overlay POC side-face hooks: north="
            + std::string(mFaces[0].installed ? "ok" : "fail")
            + ", south=" + (mFaces[1].installed ? "ok" : "fail")
            + ", west=" + (mFaces[2].installed ? "ok" : "fail")
            + ", east=" + (mFaces[3].installed ? "ok" : "fail"));
    }

    if (!any) {
        if (sInstance == this) sInstance = nullptr;
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

    if (sInstance == this) sInstance = nullptr;
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

    // Always preserve vanilla/base face first.
    original(blockTessellator, tessellator, block, position, inputTexture);

    if (!mEnabled.load(std::memory_order_relaxed)
        || !blockTessellator
        || !tessellator
        || !block
        || !position
        || !inputTexture
        || isPlank(block)
        || isGlassLike(block)) {
        return;
    }

    auto* shape = reinterpret_cast<AabbRaw*>(
        reinterpret_cast<uintptr_t>(blockTessellator)
        + kCurrentShapeAabbOffset);

    const AabbRaw originalShape = *shape;
    if (!isFullCube(originalShape)) return;

    const BlockPos pos{
        static_cast<int32_t>(std::floor(position->x)),
        static_cast<int32_t>(std::floor(position->y)),
        static_cast<int32_t>(std::floor(position->z)),
    };

    // left, down, right, up. Each edge can source a different vanilla plank.
    for (uint8_t edge = 0; edge < 4; ++edge) {
        emitNeighborOverlay(
            face,
            edge,
            blockTessellator,
            tessellator,
            block,
            pos,
            *position,
            originalShape);
    }

    // Be defensive even if an early return is introduced later.
    *shape = originalShape;
}

bool PlankOverlayHook::emitNeighborOverlay(
    uint8_t face,
    uint8_t localEdge,
    void* blockTessellator,
    void* tessellator,
    const void* targetBlock,
    const BlockPos& targetPos,
    const Vec3Raw& renderPos,
    const AabbRaw& originalShape
) {
    auto* cache = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(blockTessellator)
        + kBlockTessellatorCacheOffset);

    const BlockPos sourcePos = neighborPos(targetPos, face, localEdge);
    const void* sourceBlock = mGetBlock(cache, &sourcePos);
    if (!sourceBlock || !isPlank(sourceBlock)) return false;

    // Match Continuity's overlay visibility idea: do not bleed from a plank
    // whose same face is buried behind another solid-looking block. For this
    // first POC, only explicit air-like blocks count as exposed.
    const BlockPos n = faceNormal(face);
    const BlockPos sourceFront{
        sourcePos.x + n.x,
        sourcePos.y + n.y,
        sourcePos.z + n.z,
    };
    const void* front = mGetBlock(cache, &sourceFront);
    if (front && !isAirLike(front)) return false;

    const TextureUvSet* sourceTexture = mTextureHook->queryOriginalTexture(
        blockTessellator,
        &sourcePos,
        sourceBlock,
        face,
        0,
        nullptr);

    if (!sourceTexture) return false;

    const size_t index = static_cast<size_t>(face - 2);
    FaceFn original = mFaces[index].original;
    if (!original) return false;

    emitJaggedEdge(
        face,
        localEdge,
        original,
        blockTessellator,
        tessellator,
        targetBlock,
        renderPos,
        *sourceTexture,
        originalShape);

    if (!mLoggedFirstOverlay.exchange(true, std::memory_order_relaxed)
        && mLog) {
        mLog(
            "PLANK OVERLAY HIT: target="
            + std::string(blockFullName(targetBlock))
            + ", source=" + std::string(blockFullName(sourceBlock))
            + ", face=" + std::to_string(face)
            + ", edge=" + std::to_string(localEdge)
            + ", shape=("
            + std::to_string(originalShape.minX) + ","
            + std::to_string(originalShape.minY) + ","
            + std::to_string(originalShape.minZ) + ")->("
            + std::to_string(originalShape.maxX) + ","
            + std::to_string(originalShape.maxY) + ","
            + std::to_string(originalShape.maxZ) + ")");
    }

    return true;
}

void PlankOverlayHook::emitJaggedEdge(
    uint8_t face,
    uint8_t localEdge,
    FaceFn original,
    void* blockTessellator,
    void* tessellator,
    const void* targetBlock,
    const Vec3Raw& renderPos,
    const TextureUvSet& sourceTexture,
    const AabbRaw& originalShape
) {
    auto* shape = reinterpret_cast<AabbRaw*>(
        reinterpret_cast<uintptr_t>(blockTessellator)
        + kCurrentShapeAabbOffset);

    for (size_t band = 0; band < 4; ++band) {
        const float depth = kDepth[band];

        float u0 = 0.0f;
        float u1 = 1.0f;
        float v0 = 0.0f;
        float v1 = 1.0f;

        switch (localEdge) {
        case 0: // left
            u0 = 0.0f;
            u1 = depth;
            v0 = kBand[band];
            v1 = kBand[band + 1];
            break;

        case 2: // right
            u0 = 1.0f - depth;
            u1 = 1.0f;
            v0 = kBand[band];
            v1 = kBand[band + 1];
            break;

        case 1: // down
            u0 = kBand[band];
            u1 = kBand[band + 1];
            v0 = 0.0f;
            v1 = depth;
            break;

        case 3: // up
            u0 = kBand[band];
            u1 = kBand[band + 1];
            v0 = 1.0f - depth;
            v1 = 1.0f;
            break;

        default:
            continue;
        }

        AabbRaw strip = makeStrip(
            originalShape,
            face,
            u0,
            u1,
            v0,
            v1);

        offsetFace(strip, face, kFaceEpsilon);
        *shape = strip;

        original(
            blockTessellator,
            tessellator,
            targetBlock,
            &renderPos,
            &sourceTexture);
    }

    *shape = originalShape;
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
        || name->size() == 0
        || name->size() > 256
        || name->data() == nullptr) {
        return {};
    }

    return {name->data(), name->size()};
}

bool PlankOverlayHook::isPlank(const void* block) {
    const std::string_view name = blockFullName(block);
    return name.ends_with("_planks");
}

bool PlankOverlayHook::isGlassLike(const void* block) {
    std::string_view name = blockFullName(block);
    constexpr std::string_view prefix = "minecraft:";
    if (name.starts_with(prefix)) name.remove_prefix(prefix.size());

    return name == "glass"
        || name == "glass_pane"
        || name == "tinted_glass"
        || name.ends_with("_stained_glass")
        || name.ends_with("_stained_glass_pane");
}

bool PlankOverlayHook::isAirLike(const void* block) {
    if (!block) return true;

    std::string_view name = blockFullName(block);
    constexpr std::string_view prefix = "minecraft:";
    if (name.starts_with(prefix)) name.remove_prefix(prefix.size());

    return name == "air"
        || name == "cave_air"
        || name == "void_air";
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

BlockPos PlankOverlayHook::faceNormal(uint8_t face) {
    switch (face) {
    case 0: return {0, -1, 0};
    case 1: return {0, 1, 0};
    case 2: return {0, 0, -1};
    case 3: return {0, 0, 1};
    case 4: return {-1, 0, 0};
    case 5: return {1, 0, 0};
    default: return {0, 0, 0};
    }
}

PlankOverlayHook::AabbRaw PlankOverlayHook::makeStrip(
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

    // v=0 is local down; v=1 is local up.
    out.minY = lerp(original.minY, original.maxY, v0);
    out.maxY = lerp(original.minY, original.maxY, v1);

    switch (face) {
    case 2: // north: local left=+X, right=-X
        out.minX = lerp(original.maxX, original.minX, u1);
        out.maxX = lerp(original.maxX, original.minX, u0);
        break;

    case 3: // south: local left=-X, right=+X
        out.minX = lerp(original.minX, original.maxX, u0);
        out.maxX = lerp(original.minX, original.maxX, u1);
        break;

    case 4: // west: local left=-Z, right=+Z
        out.minZ = lerp(original.minZ, original.maxZ, u0);
        out.maxZ = lerp(original.minZ, original.maxZ, u1);
        break;

    case 5: // east: local left=+Z, right=-Z
        out.minZ = lerp(original.maxZ, original.minZ, u1);
        out.maxZ = lerp(original.maxZ, original.minZ, u0);
        break;

    default:
        break;
    }

    return out;
}

void PlankOverlayHook::offsetFace(
    AabbRaw& shape,
    uint8_t face,
    float epsilon
) {
    switch (face) {
    case 2:
        shape.minZ -= epsilon;
        break;
    case 3:
        shape.maxZ += epsilon;
        break;
    case 4:
        shape.minX -= epsilon;
        break;
    case 5:
        shape.maxX += epsilon;
        break;
    default:
        break;
    }
}

}
