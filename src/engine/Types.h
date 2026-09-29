#pragma once

#include <cstdint>

namespace better_grass::engine {

struct BlockPos {
    int32_t x;
    int32_t y;
    int32_t z;
};
static_assert(sizeof(BlockPos) == 12);

// The hook only forwards pointers returned by Minecraft. It never copies or
// edits TextureUVCoordinateSet, so the full internal layout is intentionally
// left opaque.
struct TextureUvSet;

}
