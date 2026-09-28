#pragma once

#include <cstdint>
#include <string>

namespace boardctl {

enum class RelayState {
    Off,
    On
};

class RelayController {
public:
    static constexpr std::uint16_t VendorId = 0x0403;
    static constexpr std::uint16_t ProductId = 0x6001;
    static constexpr unsigned char BulkOutEndpoint = 0x02;
    static constexpr int InterfaceNumber = 0;

    RelayController() = default;

    void set_channel(const std::string& serial, std::uint8_t channel, RelayState state);
    bool get_channel(const std::string& serial, std::uint8_t channel);
    std::uint8_t get_state_mask(const std::string& serial);

private:
    static void validate_channel(std::uint8_t channel);
};

}