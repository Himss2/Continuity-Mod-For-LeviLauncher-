#pragma once
#include <string_view>
#include <pl/Config.hpp>

namespace continuity_bedrock {
struct ModConfig {
    int version = 1;
    bool enabled = true;
    bool diagnostics = true;
    bool atlasProbe = true;
    bool enableLegacyCtm = false;
    int maxDiagnosticLogs = 32;
};
nlohmann::json makeDefaultConfigJson();
nlohmann::json makeConfigSchemaJson();
}

template <> struct pl::config::Schema<continuity_bedrock::ModConfig> {
    static constexpr std::string_view title = "Continuity Bedrock";
    static constexpr std::string_view description = "RE/CTM proof of concept for Minecraft Bedrock 1.26.52.3";
    static constexpr FieldSchema field(std::string_view name) {
        if (name == "version") return {.title = "Config version", .readOnly = true};
        if (name == "enabled") return {.title = "Enabled"};
        if (name == "diagnostics") return {.title = "Diagnostics", .description = "Log renderer-path counters and signature status."};
        if (name == "atlasProbe") return {.title = "Atlas probe", .description = "Resolve vanilla glass and 47 bundled CTM atlas entries lazily."};
        if (name == "enableLegacyCtm") return {.title = "Legacy CTM POC", .description = "Experimental: replace legacy glass UV using the 47-tile CTM resolver."};
        if (name == "maxDiagnosticLogs") return {.title = "Diagnostic log limit", .minimum = 0, .maximum = 256};
        return {};
    }
};
