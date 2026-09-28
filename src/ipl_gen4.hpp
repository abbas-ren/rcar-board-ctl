#pragma once

#include "profile.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace boardctl {

struct Gen4FlashOptions {
    std::string uart;
    std::string relay_serial;
    std::uint8_t relay_channel = 0;
    std::string gpio_chip = "/dev/gpiochip0";
    std::vector<unsigned int> gpio_pins;
    bool download_gpio_value = true;
    bool normal_gpio_value = false;
    std::filesystem::path image_dir;
    std::filesystem::path profile_path = "profiles/gen4.toml";
};

class Gen4Flasher {
public:
    void flash(const Gen4FlashOptions& opt);

private:
    void enter_download_mode(const Gen4FlashOptions& opt);
    void restore_normal_mode(const Gen4FlashOptions& opt);
};

}