#include "engine/LegacyCtmHook.h"
#include "engine/CtmResolver.h"
#include <pl/memory/Hook.hpp>
#include <cstddef>
#include <cstdint>

namespace continuity_bedrock::engine {

LegacyCtmHook* LegacyCtmHook::sInstance = nullptr;

namespace {
// Validated against Minecraft 1.26.52.3 and cross-checked with BedrockTools.
constexpr ptrdiff_t kBlockTessellatorCacheOffset = 0x678;
constexpr ptrdiff_t kBlockTypeOffset = 0x68;
constexpr ptrdiff_t kBlockTypeNameInfoOffset = 0x88;
constexpr ptrdiff_t kNameInfoFullNameOffset = 0x40;
constexpr ptrdiff_t kHashedStringStringOffset = 0x8;
}

bool LegacyCtmHook::install(const Addresses& a, const ModConfig& cfg, LogFn log) {
    uninstall();

    mAddresses = a;
    mConfig = cfg;
    mLog = std::move(log);
    mCacheGetBlock = reinterpret_cast<CacheGetBlockFn>(a.blockTessellatorCacheGetBlock);
    mGetTextureUv = reinterpret_cast<AtlasCache::GetTextureUvFn>(a.blockGraphicsGetTextureUv);

    mBookshelfType.store(0, std::memory_order_relaxed);
    mGetTextureCalls.store(0, std::memory_order_relaxed);
    mBookshelfFaces.store(0, std::memory_order_relaxed);
    mAtlasProbeAttempts.store(0, std::memory_order_relaxed);
    mPreAtlasCalls.store(0, std::memory_order_relaxed);
    mDiagnosticLogCount.store(0, std::memory_order_relaxed);
    mLoggedRuntimeHit.store(false, std::memory_order_relaxed);
    mLoggedAtlasReady.store(false, std::memory_order_relaxed);
    mLoggedBookshelf.store(false, std::memory_order_relaxed);

    sInstance = this;

    if (a.blockTessellatorGetTexture) {
        mGetTextureInstalled = pl::memory::hook(
            reinterpret_cast<void*>(a.blockTessellatorGetTexture),
            reinterpret_cast<void*>(&LegacyCtmHook::getTextureDetour),
            reinterpret_cast<void**>(&mOriginalGetTexture)) == 0;
    }

    if (mConfig.diagnostics) {
        logDiagnostic(
            std::string("Hook install state: _getTexture=")
            + (mGetTextureInstalled ? "installed" : "FAILED")
            + "; useNewTessellation diagnostic removed from hot path");
    }

    return mGetTextureInstalled;
}

void LegacyCtmHook::uninstall() {
    if (mGetTextureInstalled && mAddresses.blockTessellatorGetTexture) {
        pl::memory::unhook(
            reinterpret_cast<void*>(mAddresses.blockTessellatorGetTexture),
            reinterpret_cast<void*>(&LegacyCtmHook::getTextureDetour));
    }

    mGetTextureInstalled = false;
    mOriginalGetTexture = nullptr;
    if (sInstance == this) sInstance = nullptr;
}

void LegacyCtmHook::logDiagnostic(std::string message) {
    if (!mConfig.diagnostics || !mLog || mConfig.maxDiagnosticLogs <= 0) return;

    const uint32_t limit = static_cast<uint32_t>(mConfig.maxDiagnosticLogs);
    uint32_t current = mDiagnosticLogCount.load(std::memory_order_relaxed);
    while (current < limit) {
        if (mDiagnosticLogCount.compare_exchange_weak(
                current,
                current + 1,
                std::memory_order_relaxed,
                std::memory_order_relaxed)) {
            mLog(message);
            return;
        }
    }
}

const void* LegacyCtmHook::blockType(const void* block) {
    if (!block) return nullptr;
    return *reinterpret_cast<void* const*>(
        reinterpret_cast<uintptr_t>(block) + kBlockTypeOffset);
}

bool LegacyCtmHook::hasFullName(const void* block, std::string_view expected) {
    const auto* type = blockType(block);
    if (!type) return false;

    const uintptr_t stringAddress =
        reinterpret_cast<uintptr_t>(type)
        + kBlockTypeNameInfoOffset
        + kNameInfoFullNameOffset
        + kHashedStringStringOffset;

    const auto* name = reinterpret_cast<const std::string*>(stringAddress);

    // Guard the hot-path one-time validation from obviously invalid layouts.
    if (name->size() == 0 || name->size() > 128) return false;
    return std::string_view{name->data(), name->size()} == expected;
}

const TextureUvSet* LegacyCtmHook::getTextureDetour(
    void* self,
    const BlockPos* pos,
    const void* block,
    uint8_t face,
    int forcedVariant,
    const void* graphics
) {
    return sInstance
        ? sInstance->onGetTexture(self, pos, block, face, forcedVariant, graphics)
        : nullptr;
}

const TextureUvSet* LegacyCtmHook::onGetTexture(
    void* self,
    const BlockPos* pos,
    const void* block,
    uint8_t face,
    int forcedVariant,
    const void* graphics
) {
    const auto* original =
        mOriginalGetTexture(self, pos, block, face, forcedVariant, graphics);

    mGetTextureCalls.fetch_add(1, std::memory_order_relaxed);

    if (mConfig.diagnostics
        && !mLoggedRuntimeHit.load(std::memory_order_relaxed)
        && !mLoggedRuntimeHit.exchange(true, std::memory_order_relaxed)) {
        logDiagnostic("RUNTIME HIT: legacy _getTexture active; visual POC uses bookshelf only");
    }

    if (!original || !self || !pos || !block) return original;
    if (!mConfig.enableBookshelfPoc) return original;

    // Continuity built-in bookshelf rule uses faces=sides.
    if (face < 2 || face > 5) return original;

    if (!mAtlas.bookshelfReady()) {
        const uint64_t preAtlas =
            mPreAtlasCalls.fetch_add(1, std::memory_order_relaxed) + 1;

        // First side-face call, then sparse retries if resources were not ready yet.
        if (preAtlas == 1 || (preAtlas & 0x0FFFu) == 0) {
            mAtlasProbeAttempts.fetch_add(1, std::memory_order_relaxed);

            if (mAtlas.ensureBookshelfLoaded(mGetTextureUv)) {
                if (!mLoggedAtlasReady.exchange(true, std::memory_order_relaxed)) {
                    logDiagnostic("Atlas ready: vanilla bookshelf + 4 Continuity horizontal tiles");
                }
            } else if (preAtlas == 1) {
                logDiagnostic(
                    std::string("Atlas not ready on first probe: ") + mAtlas.lastError());
            }
        }

        if (!mAtlas.bookshelfReady()) return original;
    }

    if (!mAtlas.matchesVanillaBookshelf(*original)) return original;

    uintptr_t bookshelfType = mBookshelfType.load(std::memory_order_acquire);
    const auto* currentType = blockType(block);
    if (!currentType) return original;

    if (!bookshelfType) {
        if (!hasFullName(block, "minecraft:bookshelf")) return original;

        uintptr_t expected = 0;
        const uintptr_t discovered = reinterpret_cast<uintptr_t>(currentType);
        mBookshelfType.compare_exchange_strong(
            expected,
            discovered,
            std::memory_order_release,
            std::memory_order_relaxed);
        bookshelfType = mBookshelfType.load(std::memory_order_acquire);

        if (!mLoggedBookshelf.exchange(true, std::memory_order_relaxed)) {
            logDiagnostic("Bookshelf identified; horizontal Continuity replacement is active");
        }
    }

    if (reinterpret_cast<uintptr_t>(currentType) != bookshelfType) return original;
    if (!mCacheGetBlock) return original;

    auto* cache = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(self) + kBlockTessellatorCacheOffset);

    const auto directions = ctm::directionsForFace(face);
    const BlockPos left{
        pos->x + directions[0].x,
        pos->y + directions[0].y,
        pos->z + directions[0].z
    };
    const BlockPos right{
        pos->x + directions[2].x,
        pos->y + directions[2].y,
        pos->z + directions[2].z
    };

    const auto connected = [&](const BlockPos& neighborPos) {
        const void* neighbor = mCacheGetBlock(cache, &neighborPos);
        return neighbor
            && reinterpret_cast<uintptr_t>(blockType(neighbor)) == bookshelfType;
    };

    const bool leftConnected = connected(left);
    const bool rightConnected = connected(right);
    const uint8_t tile = ctm::horizontalTile(leftConnected, rightConnected);

    mBookshelfFaces.fetch_add(1, std::memory_order_relaxed);
    return &mAtlas.bookshelfTile(tile);
}

HookStats LegacyCtmHook::stats() const {
    return {
        mGetTextureCalls.load(std::memory_order_relaxed),
        mBookshelfFaces.load(std::memory_order_relaxed),
        mAtlasProbeAttempts.load(std::memory_order_relaxed)
    };
}

}
