#pragma once

#include "engine/CtmResolver.h"
#include "engine/Types.h"

#include <cstdint>
#include <span>

namespace continuity_bedrock::engine::processor {

enum class Symmetry : uint8_t {
    None,
    Opposite,
    All,
};

using GetBlockFn = const void* (*)(void*, const BlockPos*);
using GetBlockTypeFn = const void* (*)(const void*);

uint8_t horizontalVerticalTile(
    const BlockPos& pos,
    uint8_t face,
    void* blockCache,
    uintptr_t currentType,
    GetBlockFn getBlock,
    GetBlockTypeFn getBlockType
);

uint8_t verticalHorizontalTile(
    const BlockPos& pos,
    uint8_t face,
    void* blockCache,
    uintptr_t currentType,
    GetBlockFn getBlock,
    GetBlockTypeFn getBlockType
);

uint8_t verticalTile(
    const BlockPos& pos,
    uint8_t face,
    void* blockCache,
    uintptr_t currentType,
    GetBlockFn getBlock,
    GetBlockTypeFn getBlockType
);

uint8_t ctmTile(
    const BlockPos& pos,
    uint8_t face,
    void* blockCache,
    uintptr_t currentType,
    GetBlockFn getBlock,
    GetBlockTypeFn getBlockType
);

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
);

size_t repeatTile(
    const BlockPos& pos,
    uint8_t face,
    int width,
    int height,
    Symmetry symmetry
);

}
