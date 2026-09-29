#pragma once

#include "engine/Types.h"

#include <cstdint>
#include <string_view>

namespace continuity_bedrock::engine {

class BetterGrassResolver {
public:
    using CacheGetBlockFn = const void* (*)(void*, const BlockPos*);
    using GetTextureFn =
        const TextureUvSet* (*)(void*, const BlockPos*, const void*, uint8_t, int, const void*);

    const TextureUvSet* process(
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
    ) const;

private:
    static std::string_view blockFullName(const void* block);
    static bool isGrassBlock(const void* block);
    static BlockPos fancySupportPos(const BlockPos& pos, uint8_t face);
};

}
