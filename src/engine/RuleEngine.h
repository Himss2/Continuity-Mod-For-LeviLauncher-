#pragma once

#include "engine/Types.h"
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace continuity_bedrock::engine {

enum class RuleMethod : uint8_t {
    Horizontal,
};

enum class ConnectMode : uint8_t {
    Block,
};

struct RuleDefinition {
    std::string sourcePath;
    RuleMethod method{RuleMethod::Horizontal};
    ConnectMode connect{ConnectMode::Block};
    std::vector<std::string> matchBlocks;
    std::vector<std::string> matchTiles;
    std::vector<std::string> tiles;
    uint8_t faceMask{0x3F};
    bool innerSeams{false};
};

class RuleEngine {
public:
    using LogFn = std::function<void(const std::string&)>;
    using CacheGetBlockFn = const void* (*)(void*, const BlockPos*);
    using GetTextureUvFn = TextureUvSet (*)(const std::string&, int, int);

    bool load(const std::filesystem::path& rulesDir, LogFn log);
    size_t ruleCount() const { return mRules.size(); }

    const TextureUvSet* process(
        void* blockCache,
        const BlockPos& pos,
        const void* block,
        uint8_t face,
        const TextureUvSet& original,
        CacheGetBlockFn getBlock,
        GetTextureUvFn getTextureUv,
        uint64_t hookCallCount
    );

private:
    struct RuntimeRule {
        RuleDefinition definition;

        std::mutex atlasMutex;
        std::atomic_bool atlasReady{false};
        std::atomic<uint64_t> nextAtlasRetry{0};
        std::vector<TextureUvSet> matchTileUvs;
        std::vector<TextureUvSet> tileUvs;
        std::atomic_bool loggedAtlasReady{false};
        std::atomic_bool loggedApplied{false};
    };

    bool ensureAtlas(RuntimeRule& rule, GetTextureUvFn getTextureUv, uint64_t hookCallCount);
    bool sourceTileMatches(const RuntimeRule& rule, const TextureUvSet& original) const;
    bool blockRuleMatches(const RuntimeRule& rule, std::string_view fullName) const;

    static const void* blockType(const void* block);
    static std::string_view blockFullName(const void* block);

    std::vector<std::unique_ptr<RuntimeRule>> mRules;
    LogFn mLog;
};

}
