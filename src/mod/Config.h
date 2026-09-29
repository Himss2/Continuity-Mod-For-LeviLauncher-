#pragma once

#include <string_view>
#include <pl/Config.hpp>

namespace better_grass {

struct ModConfig {
    int version = 1;
    bool enabled = true;
    bool diagnostics = true;
};

nlohmann::json makeDefaultConfigJson();
nlohmann::json makeConfigSchemaJson();

}

template <> struct pl::config::Schema<better_grass::ModConfig> {
    static constexpr std::string_view title = "Better Grass";
    static constexpr std::string_view description =
        "Fancy Better Grass for Minecraft Bedrock.";

    static constexpr FieldSchema field(std::string_view name) {
        if (name == "version")
            return {.title = "Config version", .readOnly = true};
        if (name == "enabled")
            return {
                .title = "Enabled",
                .description = "Enable Fancy Better Grass rendering."
            };
        if (name == "diagnostics")
            return {
                .title = "Diagnostics",
                .description = "Emit one-time renderer diagnostics."
            };
        return {};
    }
};
