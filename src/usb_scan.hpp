#pragma once

#include <string>

namespace boardctl {

class UsbScanner {
public:
    static bool find_usb_tty(const std::string& device_path, std::string& tty_path);
    static int check_usb_device(const std::string& vendor_id,
                                const std::string& product_id,
                                bool first_only,
                                std::string& tty_path);
};

}