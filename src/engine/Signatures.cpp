#include "engine/Signatures.h"
#include <pl/memory/Signature.hpp>

namespace continuity_bedrock::engine {
static constexpr auto kModule = "libminecraftpe.so";

Addresses resolveAddresses() {
    Addresses a{};
    a.blockTessellatorGetTexture = pl::memory::resolveSignature(
        "FF C3 01 D1 FD 7B 02 A9 F9 1B 00 F9 F8 5F 04 A9 F6 57 05 A9 F4 4F 06 A9 FD 83 00 91 59 D0 3B D5 F3 03 05 AA F4 03 04 2A", kModule);
    a.blockTessellatorCacheGetBlock = pl::memory::resolveSignature(
        "FD 7B BB A9 F9 0B 00 F9 F8 5F 02 A9 F6 57 03 A9 F4 4F 04 A9 FD 03 00 91 F4 03 00 AA 00 00 40 F9", kModule);
    a.blockGraphicsGetTextureUv = pl::memory::resolveSignature(
        "FF 83 01 D1 FD 7B 02 A9 F8 5F 03 A9 F6 57 04 A9 F4 4F 05 A9 FD 83 00 91 58 D0 3B D5 F4 03 08 AA F7 03 00 AA 08 17 40 F9 A8 83 1F F8", kModule);
    a.useNewTessellation = pl::memory::resolveSignature(
        "FD 7B BE A9 F4 4F 01 A9 FD 03 00 91 F4 03 01 2A F3 03 00 AA DE 63 33 94", kModule);
    a.renderChunkBuilderBuild = pl::memory::resolveSignature(
        "E8 0F 19 FC FD 7B 01 A9 FC 6F 02 A9 FA 67 03 A9 F8 5F 04 A9 F6 57 05 A9 F4 4F 06 A9 FD 43 00 91 FF 43 2C D1", kModule);
    a.pipelineRun = pl::memory::resolveSignature(
        "FF 43 02 D1 FD 7B 05 A9 F8 5F 06 A9 F6 57 07 A9 F4 4F 08 A9 FD 43 01 91 57 D0 3B D5", kModule);
    return a;
}

bool coreAddressesReady(const Addresses& a) {
    return a.blockTessellatorGetTexture && a.blockTessellatorCacheGetBlock && a.blockGraphicsGetTextureUv;
}
}
