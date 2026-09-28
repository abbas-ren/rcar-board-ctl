#include "relay.hpp"
#include "util.hpp"

#include <libusb-1.0/libusb.h>
#include <cstring>
#include <memory>

namespace boardctl {

namespace {

struct LibusbContextDeleter {
    void operator()(libusb_context* ctx) const {
        if (ctx) libusb_exit(ctx);
    }
};

struct DeviceHandleDeleter {
    void operator()(libusb_device_handle* handle) const {
        if (handle) libusb_close(handle);
    }
};

class ClaimedDevice {
public:
    explicit ClaimedDevice(libusb_device_handle* h) : handle_(h), claimed_(false) {}

    void claim() {
        if (libusb_kernel_driver_active(handle_, RelayController::InterfaceNumber) == 1) {
            libusb_detach_kernel_driver(handle_, RelayController::InterfaceNumber);
        }

        int ret = libusb_claim_interface(handle_, RelayController::InterfaceNumber);
        if (ret != 0) {
            throw Error("Could not claim USB interface 0: " + std::to_string(ret));
        }
        claimed_ = true;

        ret = libusb_control_transfer(
            handle_,
            0x40,
            0x0B,
            0x01FF,
            0x01,
            nullptr,
            0,
            1000
        );
        if (ret < 0) {
            throw Error("Failed to enable FTDI bitbang mode: " + std::to_string(ret));
        }
    }

    ~ClaimedDevice() {
        if (claimed_) {
            libusb_release_interface(handle_, RelayController::InterfaceNumber);
        }
    }

private:
    libusb_device_handle* handle_;
    bool claimed_;
};

libusb_device_handle* find_device(libusb_context* ctx, const std::string& serial) {
    libusb_device** devs = nullptr;
    const ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        throw Error("libusb_get_device_list failed");
    }

    libusb_device_handle* result = nullptr;

    for (ssize_t i = 0; i < cnt; ++i) {
        libusb_device* dev = devs[i];
        libusb_device_descriptor desc{};
        if (libusb_get_device_descriptor(dev, &desc) < 0) {
            continue;
        }

        if (desc.idVendor != RelayController::VendorId ||
            desc.idProduct != RelayController::ProductId) {
            continue;
        }

        libusb_device_handle* tmp = nullptr;
        if (libusb_open(dev, &tmp) != 0) {
            continue;
        }

        unsigned char buf[256] = {};
        bool matched = false;

        if (desc.iSerialNumber) {
            int ret = libusb_get_string_descriptor_ascii(
                tmp, desc.iSerialNumber, buf, sizeof(buf));
            if (ret > 0) {
                if (serial == reinterpret_cast<char*>(buf)) {
                    matched = true;
                }
            }
        }

        if (matched) {
            result = tmp;
            break;
        } else {
            libusb_close(tmp);
        }
    }

    libusb_free_device_list(devs, 1);
    return result;
}

std::uint8_t read_state(libusb_device_handle* handle) {
    unsigned char buf[2] = {0, 0};

    int ret = libusb_control_transfer(
        handle,
        0xC0,
        0x0C,
        0x0000,
        0x01,
        buf,
        sizeof(buf),
        500
    );
    if (ret <= 0) {
        throw Error("Failed to read relay state: " + std::to_string(ret));
    }

    return buf[0];
}

void write_state(libusb_device_handle* handle, std::uint8_t state) {
    int transferred = 0;
    unsigned char out = state;
    int ret = libusb_bulk_transfer(
        handle,
        RelayController::BulkOutEndpoint,
        &out,
        1,
        &transferred,
        500
    );
    if (ret != 0 || transferred != 1) {
        throw Error("Failed to write relay state");
    }
}

} // namespace

void RelayController::validate_channel(std::uint8_t channel) {
    if (channel > 7) {
        throw Error("Relay channel must be in range 0..7");
    }
}

std::uint8_t RelayController::get_state_mask(const std::string& serial) {
    libusb_context* raw_ctx = nullptr;
    if (libusb_init(&raw_ctx) < 0) {
        throw Error("Failed to initialize libusb");
    }
    std::unique_ptr<libusb_context, LibusbContextDeleter> ctx(raw_ctx);

    libusb_device_handle* raw_handle = find_device(ctx.get(), serial);
    if (!raw_handle) {
        throw Error("Relay device not found for serial: " + serial);
    }
    std::unique_ptr<libusb_device_handle, DeviceHandleDeleter> handle(raw_handle);

    ClaimedDevice claimed(handle.get());
    claimed.claim();

    return read_state(handle.get());
}

bool RelayController::get_channel(const std::string& serial, std::uint8_t channel) {
    validate_channel(channel);
    const std::uint8_t mask = get_state_mask(serial);
    return ((mask >> channel) & 0x01U) != 0;
}

void RelayController::set_channel(const std::string& serial, std::uint8_t channel, RelayState state) {
    validate_channel(channel);

    libusb_context* raw_ctx = nullptr;
    if (libusb_init(&raw_ctx) < 0) {
        throw Error("Failed to initialize libusb");
    }
    std::unique_ptr<libusb_context, LibusbContextDeleter> ctx(raw_ctx);

    libusb_device_handle* raw_handle = find_device(ctx.get(), serial);
    if (!raw_handle) {
        throw Error("Relay device not found for serial: " + serial);
    }
    std::unique_ptr<libusb_device_handle, DeviceHandleDeleter> handle(raw_handle);

    ClaimedDevice claimed(handle.get());
    claimed.claim();

    std::uint8_t current = read_state(handle.get());
    const std::uint8_t bit = static_cast<std::uint8_t>(1U << channel);

    if (state == RelayState::On) {
        current = static_cast<std::uint8_t>(current | bit);
    } else {
        current = static_cast<std::uint8_t>(current & ~bit);
    }

    write_state(handle.get(), current);
}

}