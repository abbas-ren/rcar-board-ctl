#pragma once

#include <string>
#include <vector>

namespace boardctl {

class Process {
public:
    static int run(const std::string& path, const std::vector<std::string>& args);
};

}