#include "engine/RuleEngine.h"

#include "engine/CtmResolver.h"
#include "util/Properties.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <unordered_map>

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

bool hasUnsupportedStatePredicate(std::string_view value) {
    return value.find('[') != std::string_view::npos
        || value.find('=') != std::string_view::npos;
}

bool isGlassTarget(std::string_view raw) {
    std::string value = lower(std::string(raw));

    const size_t colon = value.find(':');
    if (colon != std::string::npos) value.erase(0, colon + 1);

    const size_t slash = value.find_last_of("/\\");
    if (slash != std::string::npos) value.erase(0, slash + 1);

    if (value.ends_with(".png")) value.resize(value.size() - 4);

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

bool parseBool(
    const util::Properties& props,
    std::string_view key,
    bool fallback
) {
    const auto it = props.find(std::string(key));
    if (it == props.end()) return fallback;

    const auto value = lower(trim(it->second));
    if (value == "true" || value == "1" || value == "yes") return true;
    if (value == "false" || value == "0" || value == "no") return false;
    return fallback;
}

processor::Symmetry parseSymmetry(
    const util::Properties& props,
    bool& valid
) {
    valid = true;
    const auto it = props.find("symmetry");
    if (it == props.end()) return processor::Symmetry::None;

    const std::string value = lower(trim(it->second));
    if (value == "none") return processor::Symmetry::None;
    if (value == "opposite") return processor::Symmetry::Opposite;
    if (value == "all") return processor::Symmetry::All;

    valid = false;
    return processor::Symmetry::None;
}

std::vector<int> parseWeights(
    const std::string& value,
    bool& valid
) {
    valid = true;
    std::vector<int> weights;

    for (const auto& token : splitList(value)) {
        const size_t dash = token.find('-');

        if (dash != std::string::npos) {
            int first = 0;
            int last = 0;
            if (!parseInt(std::string_view(token).substr(0, dash), first)
                || !parseInt(std::string_view(token).substr(dash + 1), last)
                || first <= 0
                || last < first
                || last - first > 4096) {
                valid = false;
                return {};
            }

            for (int weight = first; weight <= last; ++weight) {
                weights.push_back(weight);
            }
        } else {
            int weight = 0;
            if (!parseInt(token, weight) || weight <= 0) {
                valid = false;
                return {};
            }
            weights.push_back(weight);
        }
    }

    return weights;
}

bool normalizeWeights(
    std::vector<int>& weights,
    size_t tileCount,
    int& weightSum
) {
    weightSum = 0;
    if (weights.empty()) return true;

    if (weights.size() > tileCount) {
        weights.resize(tileCount);
    }

    for (int weight : weights) {
        if (weight <= 0 || weightSum > std::numeric_limits<int>::max() - weight) {
            return false;
        }
        weightSum += weight;
    }

    if (weights.size() < tileCount) {
        if (weights.empty()) return true;

        const int average = weightSum / static_cast<int>(weights.size());
        if (average <= 0) return false;

        while (weights.size() < tileCount) {
            if (weightSum > std::numeric_limits<int>::max() - average) return false;
            weights.push_back(average);
            weightSum += average;
        }
    }

    return weightSum > 0;
}

bool plausible(const TextureUvSet& value) {
    return value.u1 > value.u0
        && value.v1 > value.v0
        && value.texWidth > 0
        && value.texHeight > 0;
}

bool requiresConnection(RuleMethod method) {
    switch (method) {
    case RuleMethod::Horizontal:
    case RuleMethod::Vertical:
    case RuleMethod::HorizontalVertical:
    case RuleMethod::VerticalHorizontal:
    case RuleMethod::Top:
    case RuleMethod::Ctm:
        return true;
    default:
        return false;
    }
}

bool requiresOrientationNone(RuleMethod method) {
    switch (method) {
    case RuleMethod::Horizontal:
    case RuleMethod::Vertical:
    case RuleMethod::HorizontalVertical:
    case RuleMethod::VerticalHorizontal:
    case RuleMethod::Repeat:
    case RuleMethod::Ctm:
        return true;
    default:
        return false;
    }
}

std::optional<RuleMethod> parseMethod(std::string value) {
    value = lower(trim(std::move(value)));

    if (value == "fixed") return RuleMethod::Fixed;
    if (value == "horizontal" || value == "bookshelf") return RuleMethod::Horizontal;
    if (value == "vertical") return RuleMethod::Vertical;
    if (value == "horizontal+vertical" || value == "h+v")
        return RuleMethod::HorizontalVertical;
    if (value == "vertical+horizontal" || value == "v+h")
        return RuleMethod::VerticalHorizontal;
    if (value == "random") return RuleMethod::Random;
    if (value == "repeat") return RuleMethod::Repeat;
    if (value == "top") return RuleMethod::Top;
    if (value == "ctm" || value == "glass") return RuleMethod::Ctm;

    return std::nullopt;
}

std::optional<size_t> exactTileCount(RuleMethod method) {
    switch (method) {
    case RuleMethod::Fixed: return 1;
    case RuleMethod::Horizontal: return 4;
    case RuleMethod::Vertical: return 4;
    case RuleMethod::HorizontalVertical: return 7;
    case RuleMethod::VerticalHorizontal: return 7;
    case RuleMethod::Top: return 1;
    case RuleMethod::Ctm: return 47;
    default: return std::nullopt;
    }
}

std::optional<RuleDefinition> compileRule(
    const std::filesystem::path& path,
    const util::Properties& props,
    std::string& error
) {
    RuleDefinition rule;
    rule.sourcePath = path.string();

    const auto method = parseMethod(
        props.contains("method") ? props.at("method") : "ctm");
    if (!method) {
        error = "unsupported method";
        return std::nullopt;
    }
    rule.method = *method;

    if (const auto it = props.find("matchBlocks"); it != props.end()) {
        rule.matchBlocks = splitList(it->second);
        for (auto& block : rule.matchBlocks) {
            if (hasUnsupportedStatePredicate(block)) {
                error = "block-state predicates are not supported yet: " + block;
                return std::nullopt;
            }
            block = normalizeBlockName(std::move(block));
        }
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
    if (rule.tiles.empty()) {
        error = "tiles resolved to an empty list";
        return std::nullopt;
    }

    for (const auto& tile : rule.tiles) {
        if (!tile.empty() && tile.front() == '<') {
            error = "special tiles such as <skip>/<default> are not supported yet";
            return std::nullopt;
        }
    }

    if (const auto exact = exactTileCount(rule.method);
        exact && rule.tiles.size() != *exact) {
        error =
            "method requires exactly " + std::to_string(*exact)
            + " tiles, got " + std::to_string(rule.tiles.size());
        return std::nullopt;
    }

    if (requiresConnection(rule.method)) {
        const std::string connect = lower(
            props.contains("connect")
                ? props.at("connect")
                : (rule.matchBlocks.empty() ? "tile" : "block"));

        if (connect == "block") {
            rule.connect = ConnectMode::Block;
        } else if (connect == "tile") {
            if (rule.method != RuleMethod::Top) {
                error = "connect=tile is currently enabled for method=top only";
                return std::nullopt;
            }
            rule.connect = ConnectMode::Tile;
        } else {
            error = "current stage supports connect=block and top/connect=tile";
            return std::nullopt;
        }
    }

    bool facesValid = false;
    rule.faceMask = parseFaces(
        props.contains("faces") ? props.at("faces") : "all",
        facesValid);
    if (!facesValid) {
        error = "invalid faces";
        return std::nullopt;
    }

    rule.innerSeams = parseBool(props, "innerSeams", false);

    if (requiresOrientationNone(rule.method)) {
        const std::string orient =
            lower(props.contains("orient") ? props.at("orient") : "none");
        if (orient != "none") {
            error = "current stage supports orient=none only";
            return std::nullopt;
        }
    }

    bool symmetryValid = false;
    rule.symmetry = parseSymmetry(props, symmetryValid);
    if (!symmetryValid) {
        error = "invalid symmetry";
        return std::nullopt;
    }

    if (rule.method == RuleMethod::Random) {
        if (const auto it = props.find("randomLoops"); it != props.end()) {
            if (!parseInt(trim(it->second), rule.randomLoops)
                || rule.randomLoops < 0
                || rule.randomLoops > 9) {
                error = "randomLoops must be between 0 and 9";
                return std::nullopt;
            }
        }

        rule.linked = parseBool(props, "linked", false);

        if (const auto it = props.find("weights"); it != props.end()) {
            bool weightsValid = false;
            rule.weights = parseWeights(it->second, weightsValid);
            if (!weightsValid
                || !normalizeWeights(
                    rule.weights,
                    rule.tiles.size(),
                    rule.weightSum)) {
                error = "invalid random weights";
                return std::nullopt;
            }
        }
    }

    if (rule.method == RuleMethod::Repeat) {
        const auto widthIt = props.find("width");
        const auto heightIt = props.find("height");

        if (widthIt == props.end()
            || !parseInt(trim(widthIt->second), rule.width)
            || rule.width <= 0) {
            error = "repeat requires positive width";
            return std::nullopt;
        }

        if (heightIt == props.end()
            || !parseInt(trim(heightIt->second), rule.height)
            || rule.height <= 0) {
            error = "repeat requires positive height";
            return std::nullopt;
        }

        const uint64_t required =
            static_cast<uint64_t>(rule.width)
            * static_cast<uint64_t>(rule.height);

        if (required != rule.tiles.size()) {
            error =
                "repeat requires width*height="
                + std::to_string(required)
                + " tiles, got "
                + std::to_string(rule.tiles.size());
            return std::nullopt;
        }
    }

    return rule;
}

std::string methodName(RuleMethod method) {
    switch (method) {
    case RuleMethod::Fixed: return "fixed";
    case RuleMethod::Horizontal: return "horizontal";
    case RuleMethod::Vertical: return "vertical";
    case RuleMethod::HorizontalVertical: return "horizontal+vertical";
    case RuleMethod::VerticalHorizontal: return "vertical+horizontal";
    case RuleMethod::Random: return "random";
    case RuleMethod::Repeat: return "repeat";
    case RuleMethod::Top: return "top";
    case RuleMethod::Ctm: return "ctm";
    }
    return "?";
}

}

bool RuleEngine::load(
    const std::filesystem::path& rulesDir,
    LogFn log
) {
    mRules.clear();
    ++mGeneration;
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
            if (mLog) {
                mLog(
                    "Rule skipped: "
                    + path.filename().string()
                    + " (" + error + ")");
            }
            continue;
        }

        auto runtime = std::make_unique<RuntimeRule>();
        runtime->definition = std::move(*definition);
        mRules.emplace_back(std::move(runtime));
    }

    if (mLog) {
        std::string methods;
        for (size_t i = 0; i < mRules.size(); ++i) {
            if (i) methods += ",";
            methods += methodName(mRules[i]->definition.method);
        }

        mLog(
            "Rule compiler: loaded=" + std::to_string(mRules.size())
            + ", skipped=" + std::to_string(skipped)
            + ", methods=[" + methods + "]");
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
    if (name->size() == 0
        || name->size() > 256
        || name->data() == nullptr) {
        return {};
    }

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
        [&](const std::string& value) {
            return value == fullName;
        });
}

std::vector<RuleEngine::RuntimeRule*> RuleEngine::resolveCandidates(
    uintptr_t currentBlockType,
    const void* block
) const {
    std::vector<RuntimeRule*> out;

    const std::string_view fullName = blockFullName(block);
    if (fullName.empty()) return out;

    for (const auto& rule : mRules) {
        if (blockRuleMatches(*rule, fullName)) {
            out.push_back(rule.get());
        }
    }

    return out;
}

bool RuleEngine::ensureAtlas(
    RuntimeRule& rule,
    GetTextureUvFn getTextureUv,
    uint64_t hookCallCount
) {
    if (rule.atlasReady.load(std::memory_order_acquire)) return true;
    if (!getTextureUv) return false;

    if (hookCallCount < rule.nextAtlasRetry.load(std::memory_order_relaxed)) {
        return false;
    }

    std::scoped_lock lock(rule.atlasMutex);

    if (rule.atlasReady.load(std::memory_order_relaxed)) return true;
    if (hookCallCount < rule.nextAtlasRetry.load(std::memory_order_relaxed)) {
        return false;
    }

    std::vector<TextureUvSet> matchUvs;
    matchUvs.reserve(rule.definition.matchTiles.size());

    for (const auto& key : rule.definition.matchTiles) {
        auto uv = getTextureUv(key, 0, 0);
        if (!plausible(uv)) {
            rule.nextAtlasRetry.store(
                hookCallCount + 4096,
                std::memory_order_relaxed);
            return false;
        }
        matchUvs.emplace_back(std::move(uv));
    }

    std::vector<TextureUvSet> tileUvs;
    tileUvs.reserve(rule.definition.tiles.size());

    for (const auto& key : rule.definition.tiles) {
        auto uv = getTextureUv(key, 0, 0);
        if (!plausible(uv)) {
            rule.nextAtlasRetry.store(
                hookCallCount + 4096,
                std::memory_order_relaxed);
            return false;
        }
        tileUvs.emplace_back(std::move(uv));
    }

    rule.matchTileUvs = std::move(matchUvs);
    rule.tileUvs = std::move(tileUvs);
    rule.atlasReady.store(true, std::memory_order_release);

    if (!rule.loggedAtlasReady.exchange(true, std::memory_order_relaxed)
        && mLog) {
        mLog(
            "Rule atlas ready: "
            + std::filesystem::path(rule.definition.sourcePath).filename().string()
            + ", method=" + methodName(rule.definition.method)
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
        [&](const TextureUvSet& uv) {
            return sameUvRect(uv, original);
        });
}

const TextureUvSet* RuleEngine::process(
    void* blockCache,
    const BlockPos& pos,
    const void* block,
    uint8_t face,
    const TextureUvSet& original,
    CacheGetBlockFn getBlock,
    GetTextureUvFn getTextureUv,
    GetTextureFn getTexture,
    void* tessellator,
    int forcedVariant,
    uint64_t hookCallCount
) {
    if (!block
        || face > 5
        || mRules.empty()) {
        return &original;
    }

    const uintptr_t currentType =
        reinterpret_cast<uintptr_t>(blockType(block));
    if (!currentType) return &original;

    struct ThreadCache {
        const RuleEngine* owner{};
        uint64_t generation{};
        std::unordered_map<uintptr_t, std::vector<RuntimeRule*>> byType;
    };
    thread_local ThreadCache cache;

    if (cache.owner != this || cache.generation != mGeneration) {
        cache.owner = this;
        cache.generation = mGeneration;
        cache.byType.clear();
    }

    auto it = cache.byType.find(currentType);
    if (it == cache.byType.end()) {
        it = cache.byType.emplace(
            currentType,
            resolveCandidates(currentType, block)).first;
    }

    const auto& candidates = it->second;
    if (candidates.empty()) return &original;

    for (RuntimeRule* rule : candidates) {
        if ((rule->definition.faceMask & (1u << face)) == 0) continue;
        if (!ensureAtlas(*rule, getTextureUv, hookCallCount)) continue;
        if (!sourceTileMatches(*rule, original)) continue;

        size_t tile = 0;

        switch (rule->definition.method) {
        case RuleMethod::Fixed:
            tile = 0;
            break;

        case RuleMethod::Horizontal: {
            if (!blockCache || !getBlock) continue;
            const auto d = ctm::directionsForFace(face);

            const BlockPos left{
                pos.x + d[0].x,
                pos.y + d[0].y,
                pos.z + d[0].z
            };
            const BlockPos right{
                pos.x + d[2].x,
                pos.y + d[2].y,
                pos.z + d[2].z
            };

            const auto connected = [&](const BlockPos& p) {
                const void* neighbor = getBlock(blockCache, &p);
                return neighbor
                    && reinterpret_cast<uintptr_t>(blockType(neighbor))
                        == currentType;
            };

            tile = ctm::horizontalTile(
                connected(left),
                connected(right));
            break;
        }

        case RuleMethod::Vertical:
            if (!blockCache || !getBlock) continue;
            tile = processor::verticalTile(
                pos,
                face,
                blockCache,
                currentType,
                getBlock,
                &RuleEngine::blockType);
            break;

        case RuleMethod::HorizontalVertical:
            if (!blockCache || !getBlock) continue;
            tile = processor::horizontalVerticalTile(
                pos,
                face,
                blockCache,
                currentType,
                getBlock,
                &RuleEngine::blockType);
            break;

        case RuleMethod::VerticalHorizontal:
            if (!blockCache || !getBlock) continue;
            tile = processor::verticalHorizontalTile(
                pos,
                face,
                blockCache,
                currentType,
                getBlock,
                &RuleEngine::blockType);
            break;

        case RuleMethod::Random:
            tile = processor::randomTile(
                pos,
                face,
                rule->tileUvs.size(),
                rule->definition.weights,
                rule->definition.weightSum,
                rule->definition.randomLoops,
                rule->definition.symmetry,
                rule->definition.linked,
                blockCache,
                currentType,
                getBlock,
                &RuleEngine::blockType);
            break;

        case RuleMethod::Repeat:
            tile = processor::repeatTile(
                pos,
                face,
                rule->definition.width,
                rule->definition.height,
                rule->definition.symmetry);
            break;

        case RuleMethod::Top: {
            // Continuity TopQuadProcessor defaults to Axis.Y when the block
            // has no AXIS property. Sandstone/red sandstone use this path.
            // Top/bottom faces are never replaced; side faces connect to the
            // block one position above.
            if (!blockCache || !getBlock || face < 2) continue;

            const BlockPos above{pos.x, pos.y + 1, pos.z};
            const void* neighbor = getBlock(blockCache, &above);
            if (!neighbor) continue;

            bool connected = false;

            if (rule->definition.connect == ConnectMode::Block) {
                connected =
                    reinterpret_cast<uintptr_t>(blockType(neighbor))
                    == currentType;
            } else if (rule->definition.connect == ConnectMode::Tile) {
                if (!getTexture || !tessellator) continue;

                const TextureUvSet* neighborTexture = getTexture(
                    tessellator,
                    &above,
                    neighbor,
                    face,
                    forcedVariant,
                    nullptr);

                connected =
                    neighborTexture
                    && sameUvRect(*neighborTexture, original);
            }

            if (!connected) continue;
            tile = 0;
            break;
        }

        case RuleMethod::Ctm:
            if (!blockCache || !getBlock) continue;
            tile = processor::ctmTile(
                pos,
                face,
                blockCache,
                currentType,
                getBlock,
                &RuleEngine::blockType);
            break;
        }

        if (tile >= rule->tileUvs.size()) continue;

        if (!rule->loggedApplied.exchange(true, std::memory_order_relaxed)
            && mLog) {
            mLog(
                "Rule applied: "
                + std::filesystem::path(rule->definition.sourcePath).filename().string()
                + ", method=" + methodName(rule->definition.method));
        }

        return &rule->tileUvs[tile];
    }

    return &original;
}

}
