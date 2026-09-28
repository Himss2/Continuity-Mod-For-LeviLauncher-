#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>

namespace continuity_bedrock::engine {
struct BlockPos { int32_t x, y, z; };
static_assert(sizeof(BlockPos) == 12);

// 1.26.52.3: BlockGraphics::getTextureUVCoordinateSet indexes 0x58-byte entries.
struct alignas(8) TextureUvSet {
    float weight;
    float u0;
    float v0;
    float u1;
    float v1;
    uint16_t texWidth;
    uint16_t texHeight;
    uint16_t sourceWidth;
    uint16_t sourceHeight;
    std::array<std::byte, 60> opaqueTail{};
};
static_assert(sizeof(TextureUvSet) == 0x58);

inline bool sameUvRect(const TextureUvSet& a, const TextureUvSet& b, float eps = 1e-7f) {
    return std::fabs(a.u0 - b.u0) <= eps && std::fabs(a.v0 - b.v0) <= eps &&
           std::fabs(a.u1 - b.u1) <= eps && std::fabs(a.v1 - b.v1) <= eps &&
           a.texWidth == b.texWidth && a.texHeight == b.texHeight;
}
}
