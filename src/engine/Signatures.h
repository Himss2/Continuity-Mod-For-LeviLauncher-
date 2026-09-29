#pragma once
#include <cstdint>
#include <string>

namespace continuity_bedrock::engine {
struct Addresses {
    uintptr_t blockTessellatorGetTexture{};
    uintptr_t blockTessellatorCacheGetBlock{};
    uintptr_t blockGraphicsGetTextureUv{};
    uintptr_t useNewTessellation{};
    uintptr_t renderChunkBuilderBuild{};
    uintptr_t pipelineRun{};
    uintptr_t tessellateFaceNorth{};
    uintptr_t tessellateFaceSouth{};
    uintptr_t tessellateFaceWest{};
    uintptr_t tessellateFaceEast{};
    uintptr_t textureUvCopyCtor{};
    uintptr_t textureUvDtor{};
};
Addresses resolveAddresses();
bool coreAddressesReady(const Addresses& a);
}
