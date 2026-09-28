#include "mod/Config.h"
namespace continuity_bedrock {
nlohmann::json makeDefaultConfigJson() { return pl::config::defaultJson(ModConfig{}); }
nlohmann::json makeConfigSchemaJson() { return pl::config::schema(ModConfig{}); }
}
