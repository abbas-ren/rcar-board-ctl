#include "util.hpp"

#include <fstream>

namespace boardctl {

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        out.push_back(item);
    }
    return out;
}

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const auto start = s.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    const auto end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

bool file_exists(const std::filesystem::path& p) {
    return std::filesystem::exists(p);
}

std::string read_text_file(const std::filesystem::path& p) {
    std::ifstream ifs(p);
    if (!ifs) {
        throw Error("Failed to open file: " + p.string());
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

}