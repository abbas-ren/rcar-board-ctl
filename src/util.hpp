#pragma once

#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace boardctl {

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

inline void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline void log_info(const std::string& msg) {
    std::cerr << "[INFO] " << msg << "\n";
}

inline void log_error(const std::string& msg) {
    std::cerr << "[ERROR] " << msg << "\n";
}

std::vector<std::string> split(const std::string& s, char delim);
std::string trim(const std::string& s);
bool file_exists(const std::filesystem::path& p);
std::string read_text_file(const std::filesystem::path& p);

}