#include "rtos.hpp"
#include "uart.hpp"
#include "util.hpp"

#include <chrono>
#include <fstream>

namespace boardctl {

void RtosCapture::capture(const RtosCaptureOptions& opt) {
    if (opt.device.empty()) {
        throw Error("RTOS capture requires --device");
    }

    if (opt.output.empty()) {
        throw Error("RTOS capture requires --output");
    }

    if (opt.seconds <= 0) {
        throw Error("RTOS capture --seconds must be greater than 0");
    }

    UartPort uart(opt.device);
    uart.set_baud(opt.baud);

    std::ofstream ofs(opt.output, std::ios::binary);
    if (!ofs) {
        throw Error("Failed to open RTOS output file: " + opt.output.string());
    }

    log_info("RTOS capture started: " + opt.device + " -> " + opt.output.string());

    const auto start = std::chrono::steady_clock::now();

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(now - start).count();

        if (elapsed >= opt.seconds) {
            break;
        }

        std::string chunk = uart.read_some();
        if (!chunk.empty()) {
            ofs.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
            ofs.flush();
            std::cout << chunk << std::flush;
        }

        sleep_ms(20);
    }

    log_info("RTOS capture finished");
}

}