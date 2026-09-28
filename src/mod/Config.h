#pragma once
#include <string_view>
#include <pl/Config.hpp>

namespace continuity_bedrock {
struct ModConfig {
    int version = 2;
    bool enabled = true;
    bool diagnostics = true;
    int maxDiagnosticLogs = 32;
};

nlohmann::json makeDefaultConfigJson();
nlohmann::json makeConfigSchemaJson();
}

template <> struct pl::config::Schema<continuity_bedrock::ModConfig> {
    static constexpr std::string_view title = "Continuity Bedrock";
    static constexpr std::string_view description =
        "Renderer RE/Continuity compatibility POC. Connected glass is intentionally excluded.";

    static constexpr FieldSchema field(std::string_view name) {
        if (name == "version")
            return {.title = "Config version", .readOnly = true};
        if (name == "enabled")
            return {.title = "Enabled"};
        if (name == "diagnostics")
            return {.title = "Diagnostics", .description = "Log renderer-path counters and signature status."};
        if (name == "maxDiagnosticLogs")
            return {.title = "Diagnostic log limit", .minimum = 0, .maximum = 256};
        return {};
    }
};
