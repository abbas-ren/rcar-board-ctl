#pragma once

#include <string>
#include <vector>

namespace boardctl {

class GpioController {
public:
    void set_pins(const std::string& chip_path, const std::vector<unsigned int>& pins, bool value);
};

}