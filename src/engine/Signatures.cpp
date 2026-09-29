#include "engine/Signatures.h"

#include <pl/memory/Signature.hpp>

namespace better_grass::engine {

namespace {
constexpr auto kModule = "libminecraftpe.so";
}

Addresses resolveAddresses() {
    Addresses addresses{};

    addresses.blockTessellatorGetTexture = pl::memory::resolveSignature(
        "FF C3 01 D1 FD 7B 02 A9 F9 1B 00 F9 F8 5F 04 A9 F6 57 05 A9 F4 4F 06 A9 FD 83 00 91 59 D0 3B D5 F3 03 05 AA F4 03 04 2A",
        kModule);

    addresses.blockTessellatorCacheGetBlock = pl::memory::resolveSignature(
        "FD 7B BB A9 F9 0B 00 F9 F8 5F 02 A9 F6 57 03 A9 F4 4F 04 A9 FD 03 00 91 F4 03 00 AA 00 00 40 F9",
        kModule);

    return addresses;
}

bool coreAddressesReady(const Addresses& addresses) {
    return addresses.blockTessellatorGetTexture
        && addresses.blockTessellatorCacheGetBlock;
}

}
