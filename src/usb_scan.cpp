#include "usb_scan.hpp"
#include "util.hpp"

#include <dirent.h>
#include <fstream>

namespace boardctl {

namespace {

bool find_tty_in_interface_path(const std::string& interface_path, std::string& tty_path) {
    DIR* dir = opendir(interface_path.c_str());
    if (!dir) return false;

    int best_idx = -1;
    std::string best_name;

    while (dirent* entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        if (name.rfind("ttyUSB", 0) == 0) {
            int idx = std::atoi(name.c_str() + 6);
            if (best_idx == -1 || idx < best_idx) {
                best_idx = idx;
                best_name = name;
            }
            continue;
        }

        if (name.rfind("tty", 0) == 0 && best_name.empty()) {
            best_name = name;
        }
    }

    closedir(dir);

    if (!best_name.empty()) {
        tty_path = "/dev/" + best_name;
        return true;
    }

    return false;
}

std::string read_one_line(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) return "";
    std::string s;
    std::getline(ifs, s);
    return trim(s);
}

} // namespace

bool UsbScanner::find_usb_tty(const std::string& device_path, std::string& tty_path) {
    DIR* dir = opendir(device_path.c_str());
    if (!dir) return false;

    std::string preferred;
    std::string fallback;

    while (dirent* entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (name.find(':') == std::string::npos) continue;

        const std::string interface_path = device_path + "/" + name;
        if (name.find(":1.0") != std::string::npos) {
            preferred = interface_path;
            break;
        }
        if (fallback.empty()) {
            fallback = interface_path;
        }
    }

    closedir(dir);

    if (!preferred.empty() && find_tty_in_interface_path(preferred, tty_path)) {
        return true;
    }
    if (!fallback.empty() && find_tty_in_interface_path(fallback, tty_path)) {
        return true;
    }

    return false;
}

int UsbScanner::check_usb_device(const std::string& vendor_id,
                                 const std::string& product_id,
                                 bool first_only,
                                 std::string& tty_path) {
    DIR* usb_dir = opendir("/sys/bus/usb/devices");
    if (!usb_dir) {
        throw Error("Unable to open /sys/bus/usb/devices");
    }

    int found = 0;

    while (dirent* entry = readdir(usb_dir)) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        const std::string base = "/sys/bus/usb/devices/" + name;
        const std::string vid = read_one_line(base + "/idVendor");
        if (vid != vendor_id) continue;

        const std::string pid = read_one_line(base + "/idProduct");
        if (pid != product_id) continue;

        std::string local_tty;
        if (!find_usb_tty(base, local_tty)) {
            continue;
        }

        ++found;
        tty_path = local_tty;
        if (first_only) {
            closedir(usb_dir);
            return found;
        }
    }

    closedir(usb_dir);
    return found;
}

}