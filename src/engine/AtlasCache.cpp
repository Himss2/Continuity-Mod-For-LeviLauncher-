#include "engine/AtlasCache.h"
#include <cstdio>

namespace continuity_bedrock::engine {
static bool plausible(const TextureUvSet& v) {
    return v.u1 > v.u0 && v.v1 > v.v0 && v.texWidth > 0 && v.texHeight > 0;
}
bool AtlasCache::ensureLoaded(GetTextureUvFn fn) {
    std::scoped_lock lock(mMutex);
    if (mReady) return true;
    if (!fn) { mLastError = "atlas resolver address is null"; return false; }
    mVanillaGlass = fn(std::string("glass"), 0, 0);
    if (!plausible(mVanillaGlass)) { mLastError = "vanilla glass atlas UV was not ready"; return false; }
    for (int i = 0; i < 47; ++i) {
        char key[64]; std::snprintf(key, sizeof(key), "continuity_glass_%d", i);
        mTiles[static_cast<size_t>(i)] = fn(std::string(key), 0, 0);
        if (!plausible(mTiles[static_cast<size_t>(i)])) {
            mLastError = std::string("missing/invalid atlas key: ") + key;
            return false;
        }
    }
    mReady = true;
    return true;
}
bool AtlasCache::matchesVanillaGlass(const TextureUvSet& uv) const {
    return mReady && sameUvRect(uv, mVanillaGlass);
}
}
