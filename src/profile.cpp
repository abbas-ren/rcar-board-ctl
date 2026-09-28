#include "profile.hpp"
#include "util.hpp"

#include <fstream>

namespace boardctl {

Gen4Profile ProfileLoader::load_gen4_profile(const std::filesystem::path& path) {
    if (!file_exists(path)) {
        throw Error("Profile not found: " + path.string());
    }

    // Minimal parser for the known TOML structure.
    // This avoids adding another dependency in this first implementation.
    std::ifstream ifs(path);
    if (!ifs) {
        throw Error("Failed to open profile: " + path.string());
    }

    Gen4Profile profile;
    Gen4Image current;
    bool in_image = false;

    std::string line;
    while (std::getline(ifs, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (line == "[[images]]") {
            if (in_image) {
                profile.images.push_back(current);
                current = Gen4Image{};
            }
            in_image = true;
            continue;
        }

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));

        if (!value.empty() && value.front() == '"') value.erase(0, 1);
        if (!value.empty() && value.back() == '"') value.pop_back();

        if (key == "name") profile.name = value;
        else if (key == "uart_baud") profile.uart_baud = std::stoi(value);
        else if (key == "download_prompt") profile.download_prompt = value;
        else if (key == "flashwriter_prompt") profile.flashwriter_prompt = value;
        else if (key == "file" && !in_image) profile.flashwriter_file = value;
        else if (key == "file" && in_image) current.file = value;
        else if (key == "target" && in_image) {
            current.target = (value == "emmc") ? ImageTarget::Emmc : ImageTarget::Flash;
        }
        else if (key == "save_address" && in_image) current.save_address = value;
        else if (key == "load_address" && in_image) current.load_address = value;
    }

    if (in_image) {
        profile.images.push_back(current);
    }

    if (profile.flashwriter_file.empty()) {
        throw Error("Profile missing flashwriter file");
    }

    if (profile.images.empty()) {
        throw Error("Profile contains no images");
    }

    return profile;
}

}