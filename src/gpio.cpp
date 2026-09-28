#include "gpio.hpp"
#include "util.hpp"

#include <gpiod.h>
#include <memory>

namespace boardctl {

namespace {

struct ChipDeleter {
    void operator()(gpiod_chip* chip) const {
        if (chip) gpiod_chip_close(chip);
    }
};

struct RequestDeleter {
    void operator()(gpiod_line_request* req) const {
        if (req) gpiod_line_request_release(req);
    }
};

} // namespace

void GpioController::set_pins(const std::string& chip_path,
                              const std::vector<unsigned int>& pins,
                              bool value) {
    if (pins.empty()) {
        throw Error("No GPIO pins provided");
    }

    std::unique_ptr<gpiod_chip, ChipDeleter> chip(gpiod_chip_open(chip_path.c_str()));
    if (!chip) {
        throw Error("Failed to open GPIO chip: " + chip_path);
    }

    gpiod_line_settings* settings = gpiod_line_settings_new();
    if (!settings) {
        throw Error("Failed to allocate gpiod line settings");
    }

    gpiod_line_config* line_cfg = gpiod_line_config_new();
    if (!line_cfg) {
        gpiod_line_settings_free(settings);
        throw Error("Failed to allocate gpiod line config");
    }

    gpiod_request_config* req_cfg = gpiod_request_config_new();
    if (!req_cfg) {
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        throw Error("Failed to allocate gpiod request config");
    }

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);

    if (gpiod_line_config_add_line_settings(
            line_cfg,
            pins.data(),
            pins.size(),
            settings) != 0) {
        gpiod_request_config_free(req_cfg);
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        throw Error("Failed to add GPIO line settings");
    }

    gpiod_request_config_set_consumer(req_cfg, "boardctl");

    std::unique_ptr<gpiod_line_request, RequestDeleter> req(
        gpiod_chip_request_lines(chip.get(), req_cfg, line_cfg)
    );

    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);

    if (!req) {
        throw Error("Failed to request GPIO lines");
    }

    std::vector<gpiod_line_value> values(
        pins.size(),
        value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE
    );

    if (gpiod_line_request_set_values(req.get(), values.data()) < 0) {
        throw Error("Failed to set GPIO values");
    }
}

}