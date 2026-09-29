#include "mod/Config.h"

#include <filesystem>
#include <fstream>
#include <iostream>

static bool writeJson(
    const std::filesystem::path& path,
    const nlohmann::json& value
) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return false;

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;

    file << value.dump(2) << '\n';
    return file.good();
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: levi_config_generator <dir>\n";
        return 2;
    }

    const std::filesystem::path dir = argv[1];

    return writeJson(
               dir / "config.json",
               better_grass::makeDefaultConfigJson())
        && writeJson(
               dir / "config.schema.json",
               better_grass::makeConfigSchemaJson())
        ? 0
        : 1;
}
