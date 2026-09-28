#include "engine/SimpleProcessors.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace continuity_bedrock::engine::processor {

namespace {

constexpr uint64_t kGoldenGamma = 0x9e3779b97f4a7c15ULL;

bool connectedAt(
    const BlockPos& p,
    void* blockCache,
    uintptr_t currentType,
    GetBlockFn getBlock,
    GetBlockTypeFn getBlockType
) {
    const void* neighbor = getBlock(blockCache, &p);
    return neighbor
        && reinterpret_cast<uintptr_t>(getBlockType(neighbor)) == currentType;
}

BlockPos moved(const BlockPos& pos, ctm::Vec3i d) {
    return {pos.x + d.x, pos.y + d.y, pos.z + d.z};
}

BlockPos moved2(const BlockPos& pos, ctm::Vec3i a, ctm::Vec3i b) {
    return {pos.x + a.x + b.x, pos.y + a.y + b.y, pos.z + a.z + b.z};
}

uint8_t applySymmetry(uint8_t face, Symmetry symmetry) {
    if (symmetry == Symmetry::All) return 0; // DOWN

    if (symmetry == Symmetry::Opposite) {
        // Java Direction.AxisDirection.POSITIVE: UP, SOUTH, EAST.
        switch (face) {
        case 1: return 0;
        case 3: return 2;
        case 5: return 4;
        default: break;
        }
    }
    return face;
}

uint64_t minecraftPositionHash(int x, int y, int z) {
    // Java MathHelper.hashCode(x, y, z), reproduced with uint64 wraparound.
    uint64_t value =
        static_cast<uint64_t>(static_cast<int64_t>(x) * 3129871LL)
        ^ static_cast<uint64_t>(static_cast<int64_t>(z) * 116129781LL)
        ^ static_cast<uint64_t>(static_cast<int64_t>(y));
    value = value * value * 42317861ULL + value * 11ULL;
    return static_cast<uint64_t>(static_cast<int64_t>(value) >> 16);
}

uint64_t mix64(uint64_t z) {
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

int32_t mix32(uint64_t z) {
    z = (z ^ (z >> 33)) * 0x62a9d9ed799705f5ULL;
    const uint64_t mixed = (z ^ (z >> 28)) * 0xcb24d0a5c88c35b3ULL;
    return static_cast<int32_t>(mixed >> 32);
}

int32_t continuityRandom(
    int x,
    int y,
    int z,
    uint8_t face,
    int randomLoops
) {
    const uint64_t base =
        minecraftPositionHash(x, y, z)
        ^ mix64(kGoldenGamma * static_cast<uint64_t>(1 + face));
    return mix32(base + kGoldenGamma * static_cast<uint64_t>(1 + randomLoops));
}

uint32_t removeSignBit(int32_t value) {
    return static_cast<uint32_t>(value) & 0x7FFFFFFFU;
}

}

uint8_t verticalTile(
    const BlockPos& pos,
    uint8_t face,
    void* blockCache,
    uintptr_t currentType,
    GetBlockFn getBlock,
    GetBlockTypeFn getBlockType
) {
    const auto d = ctm::directionsForFace(face);

    const bool down = connectedAt(
        moved(pos, d[1]),
        blockCache,
        currentType,
        getBlock,
        getBlockType);

    const bool up = connectedAt(
        moved(pos, d[3]),
        blockCache,
        currentType,
        getBlock,
        getBlockType);

    // Continuity VerticalSpriteProvider uses {3,2,0,1}, same as horizontal.
    return ctm::horizontalTile(down, up);
}

uint8_t horizontalVerticalTile(
    const BlockPos& pos,
    uint8_t face,
    void* blockCache,
    uintptr_t currentType,
    GetBlockFn getBlock,
    GetBlockTypeFn getBlockType
) {
    const auto d = ctm::directionsForFace(face);

    const bool left = connectedAt(
        moved(pos, d[0]),
        blockCache,
        currentType,
        getBlock,
        getBlockType);

    const bool right = connectedAt(
        moved(pos, d[2]),
        blockCache,
        currentType,
        getBlock,
        getBlockType);

    const uint8_t primary =
        static_cast<uint8_t>((left ? 1u : 0u) | (right ? 2u : 0u));

    if (primary != 0) {
        return ctm::horizontalTile(left, right);
    }

    static constexpr std::array<uint8_t, 64> kSecondary = {
        3,3,6,3,3,3,3,3,3,3,6,3,3,3,3,3,
        4,4,5,4,4,4,4,4,3,3,6,3,3,3,3,3,
        3,3,6,3,3,3,3,3,3,3,6,3,3,3,3,3,
        3,3,6,3,3,3,3,3,3,3,6,3,3,3,3,3,
    };

    uint8_t secondary = 0;

    for (int i = 0; i < 2; ++i) {
        const auto vertical = d[i * 2 + 1];

        if (!connectedAt(
                moved(pos, vertical),
                blockCache,
                currentType,
                getBlock,
                getBlockType)) {
            continue;
        }

        secondary |= static_cast<uint8_t>(1u << (i * 3 + 1));

        for (int j = 0; j < 2; ++j) {
            const auto horizontal = d[((i + j) % 2) * 2];

            if (connectedAt(
                    moved2(pos, vertical, horizontal),
                    blockCache,
                    currentType,
                    getBlock,
                    getBlockType)) {
                secondary |= static_cast<uint8_t>(1u << (i * 3 + j * 2));
            }
        }
    }

    return kSecondary[secondary];
}

uint8_t verticalHorizontalTile(
    const BlockPos& pos,
    uint8_t face,
    void* blockCache,
    uintptr_t currentType,
    GetBlockFn getBlock,
    GetBlockTypeFn getBlockType
) {
    const auto d = ctm::directionsForFace(face);

    const bool down = connectedAt(
        moved(pos, d[1]),
        blockCache,
        currentType,
        getBlock,
        getBlockType);

    const bool up = connectedAt(
        moved(pos, d[3]),
        blockCache,
        currentType,
        getBlock,
        getBlockType);

    const uint8_t primary =
        static_cast<uint8_t>((down ? 1u : 0u) | (up ? 2u : 0u));

    if (primary != 0) {
        return ctm::horizontalTile(down, up);
    }

    static constexpr std::array<uint8_t, 64> kSecondary = {
        3,6,3,3,3,6,3,3,4,5,4,4,3,6,3,3,
        3,6,3,3,3,6,3,3,3,6,3,3,3,6,3,3,
        3,3,3,3,3,3,3,3,4,4,4,4,3,3,3,3,
        3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    };

    uint8_t secondary = 0;

    for (int i = 0; i < 2; ++i) {
        const auto horizontal = d[i * 2];

        if (!connectedAt(
                moved(pos, horizontal),
                blockCache,
                currentType,
                getBlock,
                getBlockType)) {
            continue;
        }

        secondary |= static_cast<uint8_t>(1u << (i * 3));

        for (int j = 0; j < 2; ++j) {
            const auto vertical = d[((i + j) % 2) * 2 + 1];

            if (connectedAt(
                    moved2(pos, horizontal, vertical),
                    blockCache,
                    currentType,
                    getBlock,
                    getBlockType)) {
                secondary |= static_cast<uint8_t>(
                    1u << ((i * 3 + j * 2 + 5) % 6));
            }
        }
    }

    return kSecondary[secondary];
}

uint8_t ctmTile(
    const BlockPos& pos,
    uint8_t face,
    void* blockCache,
    uintptr_t currentType,
    GetBlockFn getBlock,
    GetBlockTypeFn getBlockType
) {
    const uint8_t mask = ctm::buildMask(
        pos,
        face,
        [&](const BlockPos& neighbor) {
            return connectedAt(
                neighbor,
                blockCache,
                currentType,
                getBlock,
                getBlockType);
        });

    return ctm::tileForMask(mask);
}

size_t randomTile(
    const BlockPos& pos,
    uint8_t face,
    size_t tileCount,
    std::span<const int> normalizedWeights,
    int weightSum,
    int randomLoops,
    Symmetry symmetry,
    bool linked,
    void* blockCache,
    uintptr_t currentType,
    GetBlockFn getBlock,
    GetBlockTypeFn getBlockType
) {
    if (tileCount <= 1) return 0;

    int y = pos.y;

    if (linked && blockCache && getBlock && getBlockType) {
        BlockPos cursor = pos;
        int i = 0;

        do {
            --cursor.y;
            ++i;
        } while (
            i < 3
            && connectedAt(
                cursor,
                blockCache,
                currentType,
                getBlock,
                getBlockType));

        y = cursor.y + 1;
    }

    const uint8_t symmetricFace = applySymmetry(face, symmetry);
    const int32_t random =
        continuityRandom(pos.x, y, pos.z, symmetricFace, randomLoops);
    const uint32_t value = removeSignBit(random);

    if (normalizedWeights.empty() || weightSum <= 0) {
        return static_cast<size_t>(
            value % static_cast<uint32_t>(tileCount));
    }

    int remaining =
        static_cast<int>(value % static_cast<uint32_t>(weightSum));

    size_t index = 0;
    while (index + 1 < normalizedWeights.size()
           && remaining >= normalizedWeights[index]) {
        remaining -= normalizedWeights[index];
        ++index;
    }

    return index;
}

size_t repeatTile(
    const BlockPos& pos,
    uint8_t face,
    int width,
    int height,
    Symmetry symmetry
) {
    if (width <= 0 || height <= 0) return 0;

    face = applySymmetry(face, symmetry);

    int spriteX = 0;
    int spriteY = 0;

    switch (face) {
    case 0: // DOWN
        spriteX = pos.x;
        spriteY = -pos.z - 1;
        break;

    case 1: // UP
        spriteX = pos.x;
        spriteY = pos.z;
        break;

    case 2: // NORTH
        spriteX = -pos.x - 1;
        spriteY = -pos.y;
        break;

    case 3: // SOUTH
        spriteX = pos.x;
        spriteY = -pos.y;
        break;

    case 4: // WEST
        spriteX = pos.z;
        spriteY = -pos.y;
        break;

    case 5: // EAST
        spriteX = -pos.z - 1;
        spriteY = -pos.y;
        break;

    default:
        break;
    }

    spriteX %= width;
    if (spriteX < 0) spriteX += width;

    spriteY %= height;
    if (spriteY < 0) spriteY += height;

    return static_cast<size_t>(width * spriteY + spriteX);
}

}
