#pragma once

#include <filesystem>
#include <string>

namespace boardctl {

struct Gen5IplOptions {
    std::string script;
    std::string sdk_version;
    std::filesystem::path path;
    std::string power;
    std::string uart;
};

class Gen5IplRunner {
public:
    int run(const Gen5IplOptions& opt);
};

}