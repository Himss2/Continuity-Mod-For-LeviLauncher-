#include "mod/Config.h"
#include <filesystem>
#include <fstream>
#include <iostream>
static bool writeJson(const std::filesystem::path& p, const nlohmann::json& j) {
    std::error_code ec; std::filesystem::create_directories(p.parent_path(), ec);
    if (ec) return false;
    std::ofstream f(p, std::ios::binary | std::ios::trunc); if (!f) return false;
    f << j.dump(2) << '\n'; return f.good();
}
int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "Usage: levi_config_generator <dir>\n"; return 2; }
    std::filesystem::path d = argv[1];
    return writeJson(d / "config.json", continuity_bedrock::makeDefaultConfigJson()) &&
           writeJson(d / "config.schema.json", continuity_bedrock::makeConfigSchemaJson()) ? 0 : 1;
}
