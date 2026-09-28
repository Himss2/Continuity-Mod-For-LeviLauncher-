#pragma once
#include "engine/Types.h"
#include <array>
#include <atomic>
#include <mutex>
#include <string>

namespace continuity_bedrock::engine {

class AtlasCache {
public:
    using GetTextureUvFn = TextureUvSet (*)(const std::string&, int, int);

    bool ensureBookshelfLoaded(GetTextureUvFn fn);
    bool bookshelfReady() const {
        return mBookshelfReady.load(std::memory_order_acquire);
    }

    bool matchesVanillaBookshelf(const TextureUvSet& uv) const;
    const TextureUvSet& bookshelfTile(size_t index) const {
        return mBookshelfTiles[index];
    }
    std::string lastError() const;

private:
    mutable std::mutex mMutex;
    std::atomic_bool mBookshelfReady{false};
    std::string mLastError;
    TextureUvSet mVanillaBookshelf{};
    std::array<TextureUvSet, 4> mBookshelfTiles{};
};

}
