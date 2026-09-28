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
    uintptr_t clientInstanceUpdate{};
    uintptr_t renderChunkCoordinatorSetAllDirty{};
};
Addresses resolveAddresses();
bool coreAddressesReady(const Addresses& a);
}
