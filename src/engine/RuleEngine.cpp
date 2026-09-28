#include "engine/RuleEngine.h"

#include "engine/CtmResolver.h"
#include "util/Properties.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <sstream>

namespace continuity_bedrock::engine {

namespace {

constexpr ptrdiff_t kBlockTypeOffset = 0x68;
constexpr ptrdiff_t kBlockTypeNameInfoOffset = 0x88;
constexpr ptrdiff_t kNameInfoFullNameOffset = 0x40;
constexpr ptrdiff_t kHashedStringStringOffset = 0x8;

std::string trim(std::string value) {
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::vector<std::string> splitList(std::string value) {
    for (char& c : value) {
        if (c == ',') c = ' ';
    }

    std::vector<std::string> out;
    std::istringstream stream(value);
    std::string token;
    while (stream >> token) {
        token = trim(std::move(token));
        if (!token.empty()) out.emplace_back(std::move(token));
    }
    return out;
}

bool parseInt(std::string_view value, int& out) {
    if (value.empty()) return false;
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc{} && ptr == end;
}

std::vector<std::string> expandTileToken(const std::string& token) {
    // Supports Continuity-style compact ranges such as:
    //   0-3
    //   continuity_bookshelf_0-3
    const size_t dash = token.rfind('-');
    if (dash == std::string::npos || dash + 1 >= token.size()) return {token};

    size_t numberStart = dash;
    while (numberStart > 0
           && std::isdigit(static_cast<unsigned char>(token[numberStart - 1]))) {
        --numberStart;
    }
    if (numberStart == dash) return {token};

    int first = 0;
    int last = 0;
    if (!parseInt(std::string_view(token).substr(numberStart, dash - numberStart), first)
        || !parseInt(std::string_view(token).substr(dash + 1), last)
        || first < 0
        || last < first
        || last - first > 512) {
        return {token};
    }

    const std::string prefix = token.substr(0, numberStart);
    std::vector<std::string> out;
    out.reserve(static_cast<size_t>(last - first + 1));
    for (int i = first; i <= last; ++i) {
        out.emplace_back(prefix + std::to_string(i));
    }
    return out;
}

std::vector<std::string> parseTiles(const std::string& value) {
    std::vector<std::string> out;
    for (const auto& token : splitList(value)) {
        auto expanded = expandTileToken(token);
        out.insert(
            out.end(),
            std::make_move_iterator(expanded.begin()),
            std::make_move_iterator(expanded.end()));
    }
    return out;
}

std::string normalizeBlockName(std::string name) {
    if (name.find(':') == std::string::npos) {
        name.insert(0, "minecraft:");
    }
    return name;
}

bool isGlassTarget(std::string_view raw) {
    std::string value = lower(std::string(raw));
    const size_t colon = value.find(':');
    if (colon != std::string::npos) value.erase(0, colon + 1);

    // Strip the most common block-state suffix forms before classification.
    const size_t state = value.find_first_of("[=");
    if (state != std::string::npos) value.resize(state);

    return value == "glass"
        || value == "glass_pane"
        || value == "tinted_glass"
        || value == "hard_glass"
        || value == "hard_glass_pane"
        || value == "stained_glass"
        || value == "stained_glass_pane"
        || value == "hard_stained_glass"
        || value == "hard_stained_glass_pane"
        || value.ends_with("_stained_glass")
        || value.ends_with("_stained_glass_pane");
}

bool containsGlassTarget(const std::vector<std::string>& values) {
    return std::any_of(values.begin(), values.end(), [](const std::string& value) {
        return isGlassTarget(value);
    });
}

uint8_t parseFaces(const std::string& value, bool& valid) {
    valid = true;
    const std::string normalized = lower(trim(value));
    if (normalized.empty() || normalized == "all") return 0x3F;
    if (normalized == "sides") return 0x3C;

    uint8_t mask = 0;
    for (const auto& tokenRaw : splitList(normalized)) {
        const std::string token = lower(tokenRaw);
        if (token == "down" || token == "bottom") mask |= 1u << 0;
        else if (token == "up" || token == "top") mask |= 1u << 1;
        else if (token == "north") mask |= 1u << 2;
        else if (token == "south") mask |= 1u << 3;
        else if (token == "west") mask |= 1u << 4;
        else if (token == "east") mask |= 1u << 5;
        else {
            valid = false;
            return 0;
        }
    }

    valid = mask != 0;
    return mask;
}

bool parseBool(const util::Properties& props, std::string_view key, bool fallback) {
    const auto it = props.find(std::string(key));
    if (it == props.end()) return fallback;
    const auto value = lower(trim(it->second));
    if (value == "true" || value == "1" || value == "yes") return true;
    if (value == "false" || value == "0" || value == "no") return false;
    return fallback;
}

bool plausible(const TextureUvSet& value) {
    return value.u1 > value.u0
        && value.v1 > value.v0
        && value.texWidth > 0
        && value.texHeight > 0;
}

std::optional<RuleDefinition> compileRule(
    const std::filesystem::path& path,
    const util::Properties& props,
    std::string& error
) {
    RuleDefinition rule;
    rule.sourcePath = path.string();

    const std::string method =
        lower(props.contains("method") ? props.at("method") : "ctm");
    if (method != "horizontal" && method != "bookshelf") {
        error = "unsupported method in current stage: " + method;
        return std::nullopt;
    }
    rule.method = RuleMethod::Horizontal;

    if (const auto it = props.find("matchBlocks"); it != props.end()) {
        rule.matchBlocks = splitList(it->second);
        for (auto& block : rule.matchBlocks) block = normalizeBlockName(std::move(block));
    }
    if (const auto it = props.find("matchTiles"); it != props.end()) {
        rule.matchTiles = splitList(it->second);
    }

    if (rule.matchBlocks.empty() && rule.matchTiles.empty()) {
        error = "rule requires matchBlocks and/or matchTiles";
        return std::nullopt;
    }

    if (containsGlassTarget(rule.matchBlocks) || containsGlassTarget(rule.matchTiles)) {
        error = "glass/pane rule intentionally excluded for BedrockTools compatibility";
        return std::nullopt;
    }

    const auto tileIt = props.find("tiles");
    if (tileIt == props.end()) {
        error = "missing tiles";
        return std::nullopt;
    }
    rule.tiles = parseTiles(tileIt->second);
    if (rule.tiles.size() != 4) {
        error = "horizontal method requires exactly 4 tiles, got "
            + std::to_string(rule.tiles.size());
        return std::nullopt;
    }

    const std::string connect = lower(
        props.contains("connect")
            ? props.at("connect")
            : (rule.matchBlocks.empty() ? "tile" : "block"));
    if (connect != "block") {
        error = "current horizontal stage supports connect=block only";
        return std::nullopt;
    }
    rule.connect = ConnectMode::Block;

    bool facesValid = false;
    rule.faceMask = parseFaces(
        props.contains("faces") ? props.at("faces") : "all",
        facesValid);
    if (!facesValid) {
        error = "invalid faces";
        return std::nullopt;
    }

    rule.innerSeams = parseBool(props, "innerSeams", false);

    const std::string orient =
        lower(props.contains("orient") ? props.at("orient") : "none");
    if (orient != "none") {
        error = "current horizontal stage supports orient=none only";
        return std::nullopt;
    }

    return rule;
}

}

bool RuleEngine::load(const std::filesystem::path& rulesDir, LogFn log) {
    mRules.clear();
    mLog = std::move(log);

    std::error_code ec;
    if (!std::filesystem::exists(rulesDir, ec)) {
        if (mLog) mLog("Rule directory does not exist: " + rulesDir.string());
        return false;
    }

    std::vector<std::filesystem::path> files;
    for (std::filesystem::recursive_directory_iterator it(rulesDir, ec), end;
         it != end && !ec;
         it.increment(ec)) {
        if (!it->is_regular_file()) continue;
        if (it->path().extension() == ".properties") files.push_back(it->path());
    }

    std::sort(files.begin(), files.end());

    size_t skipped = 0;
    for (const auto& path : files) {
        const auto props = util::loadProperties(path);
        std::string error;
        auto definition = compileRule(path, props, error);
        if (!definition) {
            ++skipped;
            if (mLog) mLog("Rule skipped: " + path.filename().string() + " (" + error + ")");
            continue;
        }

        auto runtime = std::make_unique<RuntimeRule>();
        runtime->definition = std::move(*definition);
        mRules.emplace_back(std::move(runtime));
    }

    if (mLog) {
        mLog(
            "Rule compiler: loaded=" + std::to_string(mRules.size())
            + ", skipped=" + std::to_string(skipped)
            + ", processor=horizontal/connect=block");
    }

    return !mRules.empty();
}

const void* RuleEngine::blockType(const void* block) {
    if (!block) return nullptr;
    return *reinterpret_cast<void* const*>(
        reinterpret_cast<uintptr_t>(block) + kBlockTypeOffset);
}

std::string_view RuleEngine::blockFullName(const void* block) {
    const auto* type = blockType(block);
    if (!type) return {};

    const uintptr_t stringAddress =
        reinterpret_cast<uintptr_t>(type)
        + kBlockTypeNameInfoOffset
        + kNameInfoFullNameOffset
        + kHashedStringStringOffset;

    const auto* name = reinterpret_cast<const std::string*>(stringAddress);
    if (name->size() == 0 || name->size() > 256 || name->data() == nullptr) return {};
    return {name->data(), name->size()};
}

bool RuleEngine::blockRuleMatches(
    const RuntimeRule& rule,
    std::string_view fullName
) const {
    if (rule.definition.matchBlocks.empty()) return true;
    return std::any_of(
        rule.definition.matchBlocks.begin(),
        rule.definition.matchBlocks.end(),
        [&](const std::string& value) { return value == fullName; });
}

bool RuleEngine::ensureAtlas(
    RuntimeRule& rule,
    GetTextureUvFn getTextureUv,
    uint64_t hookCallCount
) {
    if (rule.atlasReady.load(std::memory_order_acquire)) return true;
    if (!getTextureUv) return false;

    const uint64_t retryAt = rule.nextAtlasRetry.load(std::memory_order_relaxed);
    if (hookCallCount < retryAt) return false;

    std::scoped_lock lock(rule.atlasMutex);
    if (rule.atlasReady.load(std::memory_order_relaxed)) return true;
    if (hookCallCount < rule.nextAtlasRetry.load(std::memory_order_relaxed)) return false;

    std::vector<TextureUvSet> matchUvs;
    matchUvs.reserve(rule.definition.matchTiles.size());
    for (const auto& key : rule.definition.matchTiles) {
        auto uv = getTextureUv(key, 0, 0);
        if (!plausible(uv)) {
            rule.nextAtlasRetry.store(hookCallCount + 4096, std::memory_order_relaxed);
            return false;
        }
        matchUvs.emplace_back(std::move(uv));
    }

    std::vector<TextureUvSet> tileUvs;
    tileUvs.reserve(rule.definition.tiles.size());
    for (const auto& key : rule.definition.tiles) {
        auto uv = getTextureUv(key, 0, 0);
        if (!plausible(uv)) {
            rule.nextAtlasRetry.store(hookCallCount + 4096, std::memory_order_relaxed);
            return false;
        }
        tileUvs.emplace_back(std::move(uv));
    }

    rule.matchTileUvs = std::move(matchUvs);
    rule.tileUvs = std::move(tileUvs);
    rule.atlasReady.store(true, std::memory_order_release);

    if (!rule.loggedAtlasReady.exchange(true, std::memory_order_relaxed) && mLog) {
        mLog(
            "Rule atlas ready: " + std::filesystem::path(rule.definition.sourcePath).filename().string()
            + ", sources=" + std::to_string(rule.matchTileUvs.size())
            + ", tiles=" + std::to_string(rule.tileUvs.size()));
    }

    return true;
}

bool RuleEngine::sourceTileMatches(
    const RuntimeRule& rule,
    const TextureUvSet& original
) const {
    if (rule.definition.matchTiles.empty()) return true;
    return std::any_of(
        rule.matchTileUvs.begin(),
        rule.matchTileUvs.end(),
        [&](const TextureUvSet& uv) { return sameUvRect(uv, original); });
}

const TextureUvSet* RuleEngine::process(
    void* blockCache,
    const BlockPos& pos,
    const void* block,
    uint8_t face,
    const TextureUvSet& original,
    CacheGetBlockFn getBlock,
    GetTextureUvFn getTextureUv,
    uint64_t hookCallCount
) {
    if (!block || !blockCache || !getBlock || face > 5 || mRules.empty()) {
        return &original;
    }

    const uintptr_t currentType = reinterpret_cast<uintptr_t>(blockType(block));
    if (!currentType) return &original;

    struct ThreadCache {
        const RuleEngine* owner{};
        uintptr_t blockType{};
        std::vector<RuntimeRule*> candidates;
    };
    thread_local ThreadCache cache;

    if (cache.owner != this || cache.blockType != currentType) {
        cache.owner = this;
        cache.blockType = currentType;
        cache.candidates.clear();

        const std::string_view fullName = blockFullName(block);
        if (fullName.empty()) return &original;

        for (const auto& rule : mRules) {
            if (blockRuleMatches(*rule, fullName)) {
                cache.candidates.push_back(rule.get());
            }
        }
    }

    if (cache.candidates.empty()) return &original;

    for (RuntimeRule* rule : cache.candidates) {
        if ((rule->definition.faceMask & (1u << face)) == 0) continue;
        if (!ensureAtlas(*rule, getTextureUv, hookCallCount)) continue;
        if (!sourceTileMatches(*rule, original)) continue;

        if (rule->definition.method == RuleMethod::Horizontal) {
            const auto directions = ctm::directionsForFace(face);
            const BlockPos left{
                pos.x + directions[0].x,
                pos.y + directions[0].y,
                pos.z + directions[0].z
            };
            const BlockPos right{
                pos.x + directions[2].x,
                pos.y + directions[2].y,
                pos.z + directions[2].z
            };

            const auto connected = [&](const BlockPos& neighborPos) {
                const void* neighbor = getBlock(blockCache, &neighborPos);
                return neighbor
                    && reinterpret_cast<uintptr_t>(blockType(neighbor)) == currentType;
            };

            const uint8_t tile = ctm::horizontalTile(connected(left), connected(right));
            if (tile >= rule->tileUvs.size()) continue;

            if (!rule->loggedApplied.exchange(true, std::memory_order_relaxed) && mLog) {
                mLog(
                    "Rule applied: "
                    + std::filesystem::path(rule->definition.sourcePath).filename().string());
            }

            return &rule->tileUvs[tile];
        }
    }

    return &original;
}

}
