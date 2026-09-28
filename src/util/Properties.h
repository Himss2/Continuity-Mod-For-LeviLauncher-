#pragma once
#include <filesystem>
#include <string>
#include <unordered_map>

namespace continuity_bedrock::util {
using Properties = std::unordered_map<std::string, std::string>;
Properties loadProperties(const std::filesystem::path& file);
}
