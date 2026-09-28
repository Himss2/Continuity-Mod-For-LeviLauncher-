#pragma once
#include "engine/Types.h"
#include <array>
#include <mutex>
#include <string>

namespace continuity_bedrock::engine {
class AtlasCache {
public:
    using GetTextureUvFn = TextureUvSet (*)(const std::string&, int, int);
    bool ensureLoaded(GetTextureUvFn fn);
    bool ready() const { return mReady; }
    bool matchesVanillaGlass(const TextureUvSet& uv) const;
    const TextureUvSet& tile(size_t i) const { return mTiles[i]; }
    const std::string& lastError() const { return mLastError; }
private:
    mutable std::mutex mMutex;
    bool mReady{};
    std::string mLastError;
    TextureUvSet mVanillaGlass{};
    std::array<TextureUvSet, 47> mTiles{};
};
}
