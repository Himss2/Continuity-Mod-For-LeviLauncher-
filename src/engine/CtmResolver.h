#pragma once
#include "engine/Types.h"
#include <array>
#include <cstdint>

namespace continuity_bedrock::engine::ctm {

struct Vec3i {
    int x;
    int y;
    int z;
};

std::array<Vec3i, 4> directionsForFace(uint8_t face);
uint8_t tileForMask(uint8_t mask);

// Java Continuity HorizontalSpriteProvider:
// bits: left=1, right=2
// map: {3, 2, 0, 1}
uint8_t horizontalTile(bool leftConnected, bool rightConnected);

template <class Connected>
uint8_t buildMask(const BlockPos& pos, uint8_t face, Connected&& connected) {
    const auto d = directionsForFace(face); // left, down, right, up
    uint8_t mask = 0;
    for (int i = 0; i < 4; ++i) {
        BlockPos p{pos.x + d[i].x, pos.y + d[i].y, pos.z + d[i].z};
        if (connected(p)) mask |= static_cast<uint8_t>(1u << (i * 2));
    }
    for (int i = 0; i < 4; ++i) {
        const int j = (i + 1) & 3;
        if ((mask & (1u << (i * 2))) && (mask & (1u << (j * 2)))) {
            BlockPos p{pos.x + d[i].x + d[j].x,
                       pos.y + d[i].y + d[j].y,
                       pos.z + d[i].z + d[j].z};
            if (connected(p)) mask |= static_cast<uint8_t>(1u << (i * 2 + 1));
        }
    }
    return mask;
}

}
