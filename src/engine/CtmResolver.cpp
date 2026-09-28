#include "engine/CtmResolver.h"

namespace continuity_bedrock::engine::ctm {
// Ported behavior from Continuity's CtmSpriteProvider. Bit layout:
// 128 64 32 / 1 * 16 / 2 4 8
static constexpr std::array<uint8_t, 256> kSpriteIndexMap = {
0,3,0,3,12,5,12,15,0,3,0,3,12,5,12,15,1,2,1,2,4,7,4,29,1,2,1,2,13,31,13,14,
0,3,0,3,12,5,12,15,0,3,0,3,12,5,12,15,1,2,1,2,4,7,4,29,1,2,1,2,13,31,13,14,
36,17,36,17,24,19,24,43,36,17,36,17,24,19,24,43,16,18,16,18,6,46,6,21,16,18,16,18,28,9,28,22,
36,17,36,17,24,19,24,43,36,17,36,17,24,19,24,43,37,40,37,40,30,8,30,34,37,40,37,40,25,23,25,45,
0,3,0,3,12,5,12,15,0,3,0,3,12,5,12,15,1,2,1,2,4,7,4,29,1,2,1,2,13,31,13,14,
0,3,0,3,12,5,12,15,0,3,0,3,12,5,12,15,1,2,1,2,4,7,4,29,1,2,1,2,13,31,13,14,
36,39,36,39,24,41,24,27,36,39,36,39,24,41,24,27,16,42,16,42,6,20,6,10,16,42,16,42,28,35,28,44,
36,39,36,39,24,41,24,27,36,39,36,39,24,41,24,27,37,38,37,38,30,11,30,32,37,38,37,38,25,33,25,26};

std::array<Vec3i, 4> directionsForFace(uint8_t face) {
    // NONE orientation, equivalent intent to Continuity DirectionMaps map[0].
    // Order: left, down, right, up. This table is isolated so face-orientation
    // parity can be corrected without touching mask/tile logic.
    switch (face) {
    case 0: return {{{-1,0,0},{0,0,-1},{1,0,0},{0,0,1}}}; // down
    case 1: return {{{-1,0,0},{0,0,1},{1,0,0},{0,0,-1}}}; // up
    case 2: return {{{1,0,0},{0,-1,0},{-1,0,0},{0,1,0}}}; // north
    case 3: return {{{-1,0,0},{0,-1,0},{1,0,0},{0,1,0}}}; // south
    case 4: return {{{0,0,-1},{0,-1,0},{0,0,1},{0,1,0}}}; // west
    case 5: return {{{0,0,1},{0,-1,0},{0,0,-1},{0,1,0}}}; // east
    default: return {{{-1,0,0},{0,-1,0},{1,0,0},{0,1,0}}};
    }
}
uint8_t tileForMask(uint8_t mask) { return kSpriteIndexMap[mask]; }
}
