#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace boardctl {

enum class ImageTarget {
    Flash,
    Emmc
};

struct Gen4Image {
    std::string file;
    ImageTarget target;
    std::string save_address;
    std::string load_address;
};

struct Gen4Profile {
    std::string name;
    int uart_baud = 921600;
    std::string download_prompt = "please send !";
    std::string flashwriter_prompt = ">";
    std::string flashwriter_file;
    std::vector<Gen4Image> images;
};

class ProfileLoader {
public:
    static Gen4Profile load_gen4_profile(const std::filesystem::path& path);
};

}