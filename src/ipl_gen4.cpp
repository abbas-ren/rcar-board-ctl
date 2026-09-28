#include "ipl_gen4.hpp"
#include "gpio.hpp"
#include "profile.hpp"
#include "relay.hpp"
#include "uart.hpp"
#include "util.hpp"

#include <fstream>

namespace boardctl {

namespace {

void send_command(UartPort& uart, const std::string& cmd) {
    uart.send_text(cmd);
    sleep_ms(100);
}

void send_file(UartPort& uart, const std::filesystem::path& file) {
    uart.send_file_lines(file);
}

void require_wait(UartPort& uart, const std::string& expected, int timeout_sec) {
    if (!uart.wait_for(expected, timeout_sec)) {
        throw Error("Timeout waiting for UART response: " + expected);
    }
}

void load_flash_writer(UartPort& uart,
                       const std::filesystem::path& base_dir,
                       const Gen4Profile& profile) {
    const auto file = base_dir / profile.flashwriter_file;
    if (!file_exists(file)) {
        throw Error("FlashWriter not found: " + file.string());
    }

    uart.set_baud(profile.uart_baud);
    send_file(uart, file);
    require_wait(uart, profile.flashwriter_prompt, 30);
}

void load_image_flash(UartPort& uart,
                      const std::filesystem::path& base_dir,
                      const Gen4Image& img) {
    const auto file = base_dir / img.file;
    if (!file_exists(file)) {
        throw Error("Image not found: " + file.string());
    }

    send_command(uart, "xls2\r\n");
    require_wait(uart, "Select (1-3)>", 10);

    send_command(uart, "1\r\n");
    require_wait(uart, "(Push Y key)", 10);

    uart.send_text("y");
    require_wait(uart, "(Push Y key)", 10);

    uart.send_text("y");
    require_wait(uart, "Please Input : H", 10);

    send_command(uart, img.load_address + "\r\n");
    require_wait(uart, "Please Input : H", 10);

    send_command(uart, img.save_address + "\r\n");
    sleep_ms(400);

    require_wait(uart, "please send ! (Motorola S-record)", 15);
    sleep_ms(400);

    send_file(uart, file);

    // Attached code accepts either "complete!" or "Clear OK?(y/n)" [4]
    if (uart.wait_for("complete!", 60)) {
        return;
    }

    if (uart.wait_for("Clear OK?(y/n)", 5)) {
        uart.send_text("y");
        require_wait(uart, ">", 10);
        return;
    }

    throw Error("Flash programming failed for image: " + img.file);
}

void load_image_emmc(UartPort& uart,
                     const std::filesystem::path& base_dir,
                     const Gen4Image& img) {
    const auto file = base_dir / img.file;
    if (!file_exists(file)) {
        throw Error("Image not found: " + file.string());
    }

    send_command(uart, "em_w\r\n");
    require_wait(uart, "(Push Y key)", 10);

    uart.send_text("y");
    require_wait(uart, "Select area(0-2)>", 10);

    send_command(uart, "1\r\n");
    require_wait(uart, "Please Input Start Address in sector :", 10);

    send_command(uart, img.save_address + "\r\n");
    require_wait(uart, "Please Input Program Start Address :", 10);

    send_command(uart, img.load_address + "\r\n");
    sleep_ms(400);

    require_wait(uart, "please send ! (Motorola S-record)", 15);
    sleep_ms(400);

    send_file(uart, file);
    require_wait(uart, "EM_W Complete!", 60);
}

} // namespace

void Gen4Flasher::enter_download_mode(const Gen4FlashOptions& opt) {
    RelayController relay;
    GpioController gpio;

    relay.set_channel(opt.relay_serial, opt.relay_channel, RelayState::Off);
    sleep_ms(2000);

    gpio.set_pins(opt.gpio_chip, opt.gpio_pins, opt.download_gpio_value);
    sleep_ms(2000);

    relay.set_channel(opt.relay_serial, opt.relay_channel, RelayState::On);
    sleep_ms(2000);
}

void Gen4Flasher::restore_normal_mode(const Gen4FlashOptions& opt) {
    RelayController relay;
    GpioController gpio;

    relay.set_channel(opt.relay_serial, opt.relay_channel, RelayState::Off);
    sleep_ms(2000);

    gpio.set_pins(opt.gpio_chip, opt.gpio_pins, opt.normal_gpio_value);
    sleep_ms(2000);

    relay.set_channel(opt.relay_serial, opt.relay_channel, RelayState::On);
    sleep_ms(2000);
}

void Gen4Flasher::flash(const Gen4FlashOptions& opt) {
    if (opt.gpio_pins.empty()) {
        throw Error("At least one GPIO pin is required for Gen4 flash");
    }
    if (opt.uart.empty()) {
        throw Error("UART device is required");
    }

    const Gen4Profile profile = ProfileLoader::load_gen4_profile(opt.profile_path);

    enter_download_mode(opt);

    UartPort uart(opt.uart);
    uart.set_baud(profile.uart_baud);

    if (!uart.wait_for(profile.download_prompt, 50)) {
        restore_normal_mode(opt);
        throw Error("Target did not enter SCIF download mode");
    }

    load_flash_writer(uart, opt.image_dir, profile);

    for (const auto& img : profile.images) {
        if (img.target == ImageTarget::Flash) {
            load_image_flash(uart, opt.image_dir, img);
        } else {
            load_image_emmc(uart, opt.image_dir, img);
        }
        sleep_ms(1000);
    }

    restore_normal_mode(opt);
}

}