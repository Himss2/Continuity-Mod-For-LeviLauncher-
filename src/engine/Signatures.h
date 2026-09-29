#pragma once

#include <cstdint>

namespace better_grass::engine {

struct Addresses {
    uintptr_t blockTessellatorGetTexture{};
    uintptr_t blockTessellatorCacheGetBlock{};
};

Addresses resolveAddresses();
bool coreAddressesReady(const Addresses& addresses);

}
