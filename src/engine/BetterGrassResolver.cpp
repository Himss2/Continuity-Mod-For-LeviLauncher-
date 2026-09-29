#include "engine/BetterGrassResolver.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace continuity_bedrock::engine {

namespace {

constexpr std::ptrdiff_t kBlockTypeOffset = 0x68;
constexpr std::ptrdiff_t kBlockTypeNameInfoOffset = 0x88;
constexpr std::ptrdiff_t kNameInfoFullNameOffset = 0x40;
constexpr std::ptrdiff_t kHashedStringStringOffset = 0x8;

}

const TextureUvSet* BetterGrassResolver::process(
    void* blockCache,
    void* blockTessellator,
    const BlockPos& pos,
    const void* block,
    uint8_t face,
    int forcedVariant,
    const void* graphics,
    const TextureUvSet* original,
    CacheGetBlockFn getBlock,
    GetTextureFn getTexture
) const {
    if (!original
        || !blockCache
        || !blockTessellator
        || !block
        || !getBlock
        || !getTexture) {
        return original;
    }

    // Fancy Better Grass only changes horizontal side faces.
    // Bedrock face mapping: 0=down, 1=up, 2=north, 3=south, 4=west, 5=east.
    if (face < 2 || face > 5 || !isGrassBlock(block)) {
        return original;
    }

    // OptiFine-style Fancy rule:
    // the side becomes grass-top only when grass continues one block down
    // and outward in the direction of the rendered face.
    const BlockPos supportPos = fancySupportPos(pos, face);
    const void* supportBlock = getBlock(blockCache, &supportPos);

    if (!isGrassBlock(supportBlock)) {
        return original;
    }

    // Ask Minecraft itself for this block's UP-face texture. This keeps the
    // result tied to the currently active terrain atlas/resource pack instead
    // of hardcoding a grass texture or atlas UV.
    const TextureUvSet* topTexture = getTexture(
        blockTessellator,
        &pos,
        block,
        1,
        forcedVariant,
        graphics);

    return topTexture ? topTexture : original;
}

std::string_view BetterGrassResolver::blockFullName(const void* block) {
    if (!block) return {};

    const void* type = *reinterpret_cast<void* const*>(
        reinterpret_cast<std::uintptr_t>(block) + kBlockTypeOffset);
    if (!type) return {};

    const auto* name = reinterpret_cast<const std::string*>(
        reinterpret_cast<std::uintptr_t>(type)
        + kBlockTypeNameInfoOffset
        + kNameInfoFullNameOffset
        + kHashedStringStringOffset);

    if (!name
        || name->empty()
        || name->size() > 256
        || name->data() == nullptr) {
        return {};
    }

    return {name->data(), name->size()};
}

bool BetterGrassResolver::isGrassBlock(const void* block) {
    const std::string_view name = blockFullName(block);
    return name == "minecraft:grass_block"
        || name == "minecraft:grass";
}

BlockPos BetterGrassResolver::fancySupportPos(
    const BlockPos& pos,
    uint8_t face
) {
    BlockPos out{pos.x, pos.y - 1, pos.z};

    switch (face) {
    case 2: --out.z; break; // north
    case 3: ++out.z; break; // south
    case 4: --out.x; break; // west
    case 5: ++out.x; break; // east
    default: break;
    }

    return out;
}

}
