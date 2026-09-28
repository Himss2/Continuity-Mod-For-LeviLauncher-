#pragma once
#include <string_view>
#include <pl/Config.hpp>

namespace continuity_bedrock {

struct ModConfig {
    int version = 4;
    bool enabled = true;
    bool enableRuleEngine = true;
    bool diagnostics = true;
    int maxDiagnosticLogs = 8;
};

nlohmann::json makeDefaultConfigJson();
nlohmann::json makeConfigSchemaJson();

}

template <> struct pl::config::Schema<continuity_bedrock::ModConfig> {
    static constexpr std::string_view title = "Continuity Bedrock";
    static constexpr std::string_view description =
        "Continuity rule engine. Connected glass is intentionally delegated to BedrockTools.";

    static constexpr FieldSchema field(std::string_view name) {
        if (name == "version")
            return {.title = "Config version", .readOnly = true};
        if (name == "enabled")
            return {.title = "Enabled"};
        if (name == "enableRuleEngine")
            return {
                .title = "Compiled rule engine",
                .description = "Enable compiled Continuity .properties rules."
            };
        if (name == "diagnostics")
            return {
                .title = "Diagnostics",
                .description = "Emit only a few one-time renderer/rule messages."
            };
        if (name == "maxDiagnosticLogs")
            return {.title = "Diagnostic log limit", .minimum = 0, .maximum = 32};
        return {};
    }
};
