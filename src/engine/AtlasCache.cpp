#include "engine/AtlasCache.h"

namespace continuity_bedrock::engine {

namespace {
bool plausible(const TextureUvSet& value) {
    return value.u1 > value.u0
        && value.v1 > value.v0
        && value.texWidth > 0
        && value.texHeight > 0;
}

bool sameRect(const TextureUvSet& a, const TextureUvSet& b) {
    return sameUvRect(a, b);
}
}

bool AtlasCache::ensureBookshelfLoaded(GetTextureUvFn fn) {
    if (bookshelfReady()) return true;

    std::scoped_lock lock(mMutex);
    if (mBookshelfReady.load(std::memory_order_relaxed)) return true;

    if (!fn) {
        mLastError = "BlockGraphics::getTextureUVCoordinateSet is null";
        return false;
    }

    const auto vanilla = fn(std::string("bookshelf"), 0, 0);
    if (!plausible(vanilla)) {
        mLastError = "vanilla bookshelf atlas entry is not ready";
        return false;
    }

    std::array<TextureUvSet, 4> tiles{};
    for (size_t i = 0; i < tiles.size(); ++i) {
        tiles[i] = fn(std::string("continuity_bookshelf_") + std::to_string(i), 0, 0);
        if (!plausible(tiles[i])) {
            mLastError = "invalid continuity_bookshelf_" + std::to_string(i) + " atlas entry";
            return false;
        }

        if (sameRect(tiles[i], vanilla)) {
            mLastError = "custom bookshelf tile resolved to vanilla bookshelf";
            return false;
        }

        for (size_t j = 0; j < i; ++j) {
            if (sameRect(tiles[i], tiles[j])) {
                mLastError = "custom bookshelf atlas entries are not distinct";
                return false;
            }
        }
    }

    mVanillaBookshelf = vanilla;
    mBookshelfTiles = tiles;
    mLastError.clear();
    mBookshelfReady.store(true, std::memory_order_release);
    return true;
}

bool AtlasCache::matchesVanillaBookshelf(const TextureUvSet& uv) const {
    return bookshelfReady() && sameUvRect(uv, mVanillaBookshelf);
}

std::string AtlasCache::lastError() const {
    std::scoped_lock lock(mMutex);
    return mLastError;
}

}
