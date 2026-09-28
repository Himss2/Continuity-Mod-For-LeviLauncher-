#include "util/Properties.h"
#include <algorithm>
#include <cctype>
#include <fstream>

namespace continuity_bedrock::util {
static std::string trim(std::string s) {
    auto notSpace=[](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}
Properties loadProperties(const std::filesystem::path& file) {
    Properties out; std::ifstream in(file); std::string line;
    while (std::getline(in, line)) {
        line = trim(line); if (line.empty() || line[0] == '#') continue;
        auto p = line.find('='); if (p == std::string::npos) continue;
        out[trim(line.substr(0,p))] = trim(line.substr(p+1));
    }
    return out;
}
}
