#pragma once

#include <filesystem>
#include <string>

namespace boardctl {

struct RtosCaptureOptions {
    std::string device;
    int baud = 115200;
    std::filesystem::path output;
    int seconds = 60;
};

class RtosCapture {
public:
    void capture(const RtosCaptureOptions& opt);
};

}