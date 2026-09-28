#include "cli.hpp"
#include "gen5_ipl.hpp"
#include "gpio.hpp"
#include "ipl_gen4.hpp"
#include "process.hpp"
#include "relay.hpp"
#include "rtos.hpp"
#include "uart.hpp"
#include "usb_scan.hpp"
#include "util.hpp"

#include <cstdlib>
#include <iostream>

namespace boardctl {

namespace {

void print_help() {
    std::cout <<
R"(boardctl - Raspberry Pi lab automation utility

Usage:
  boardctl <command> [options]

Commands:
  relay set       --serial <serial> --channel <0-7> --state <on|off>
  relay status    --serial <serial> --channel <0-7>

  gpio set        --chip /dev/gpiochip0 --pins 17,27 --value <0|1>

  uart monitor    --device /dev/ttyUSB0 --baud 921600
  uart send       --device /dev/ttyUSB0 --baud 921600 --text "help\r\n"

  usb discover    --vid 0403 --pid 6010

  ipl flash-gen4  --uart /dev/ttyUSB0
                  --relay-serial <serial>
                  --relay-channel <0-7>
                  --gpio-chip /dev/gpiochip0
                  --gpio-pins 17,27
                  --path ./ipl/gen4
                  --profile ./profiles/gen4.toml

  ipl flash-gen5  --script ./gen5_ipl.sh
                  --sdk-version <version>
                  --path ./ipl/gen5
                  --power /dev/ttyUSB2
                  --uart /dev/ttyUSB3

  gen5 power      <on|off|reboot> --device /dev/ttyUSB2

  script run      --path ./tests/smoke.sh -- --arg1 --arg2

  rtos capture    --device /dev/ttyUSB1
                  --baud 115200
                  --output ./rtos.log
                  --seconds 60

Examples:
  boardctl relay set --serial FT123456 --channel 2 --state on
  boardctl gpio set --chip /dev/gpiochip0 --pins 17,27 --value 1
  boardctl ipl flash-gen4 --uart /dev/ttyUSB0 --relay-serial FT123456 --relay-channel 2 --gpio-pins 17,27 --path ./ipl/gen4
)";
}

std::string arg_value(int& i, int argc, char** argv) {
    if (i + 1 >= argc) {
        throw Error(std::string("Missing value for argument: ") + argv[i]);
    }

    return argv[++i];
}

std::vector<unsigned int> parse_pins(const std::string& s) {
    std::vector<unsigned int> out;

    for (const auto& item : split(s, ',')) {
        const auto t = trim(item);
        if (!t.empty()) {
            out.push_back(static_cast<unsigned int>(std::stoul(t)));
        }
    }

    return out;
}

RelayState parse_relay_state(const std::string& s) {
    if (s == "on") {
        return RelayState::On;
    }

    if (s == "off") {
        return RelayState::Off;
    }

    throw Error("Relay state must be 'on' or 'off'");
}

void cmd_relay(int argc, char** argv) {
    if (argc < 3) {
        throw Error("Usage: boardctl relay <set|status> ...");
    }

    const std::string sub = argv[2];
    RelayController relay;

    if (sub == "set") {
        std::string serial;
        int channel = -1;
        std::string state;

        for (int i = 3; i < argc; ++i) {
            const std::string a = argv[i];

            if (a == "--serial") {
                serial = arg_value(i, argc, argv);
            } else if (a == "--channel") {
                channel = std::stoi(arg_value(i, argc, argv));
            } else if (a == "--state") {
                state = arg_value(i, argc, argv);
            } else {
                throw Error("Unknown relay set option: " + a);
            }
        }

        if (serial.empty() || channel < 0 || state.empty()) {
            throw Error("relay set requires --serial --channel --state");
        }

        relay.set_channel(
            serial,
            static_cast<std::uint8_t>(channel),
            parse_relay_state(state)
        );

        std::cout << "OK\n";
        return;
    }

    if (sub == "status") {
        std::string serial;
        int channel = -1;

        for (int i = 3; i < argc; ++i) {
            const std::string a = argv[i];

            if (a == "--serial") {
                serial = arg_value(i, argc, argv);
            } else if (a == "--channel") {
                channel = std::stoi(arg_value(i, argc, argv));
            } else {
                throw Error("Unknown relay status option: " + a);
            }
        }

        if (serial.empty() || channel < 0) {
            throw Error("relay status requires --serial --channel");
        }

        const bool on = relay.get_channel(serial, static_cast<std::uint8_t>(channel));
        std::cout << (on ? "on" : "off") << "\n";
        return;
    }

    throw Error("Unknown relay subcommand: " + sub);
}

void cmd_gpio(int argc, char** argv) {
    if (argc < 3) {
        throw Error("Usage: boardctl gpio set ...");
    }

    const std::string sub = argv[2];

    if (sub != "set") {
        throw Error("Only 'gpio set' is implemented");
    }

    std::string chip = "/dev/gpiochip0";
    std::vector<unsigned int> pins;
    int value = -1;

    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];

        if (a == "--chip") {
            chip = arg_value(i, argc, argv);
        } else if (a == "--pins") {
            pins = parse_pins(arg_value(i, argc, argv));
        } else if (a == "--value") {
            value = std::stoi(arg_value(i, argc, argv));
        } else {
            throw Error("Unknown gpio option: " + a);
        }
    }

    if (pins.empty() || (value != 0 && value != 1)) {
        throw Error("gpio set requires --pins and --value {0|1}");
    }

    GpioController gpio;
    gpio.set_pins(chip, pins, value == 1);

    std::cout << "OK\n";
}

void cmd_uart(int argc, char** argv) {
    if (argc < 3) {
        throw Error("Usage: boardctl uart <monitor|send> ...");
    }

    const std::string sub = argv[2];

    if (sub == "monitor") {
        std::string device;
        int baud = 115200;

        for (int i = 3; i < argc; ++i) {
            const std::string a = argv[i];

            if (a == "--device") {
                device = arg_value(i, argc, argv);
            } else if (a == "--baud") {
                baud = std::stoi(arg_value(i, argc, argv));
            } else {
                throw Error("Unknown uart monitor option: " + a);
            }
        }

        if (device.empty()) {
            throw Error("uart monitor requires --device");
        }

        UartPort uart(device);
        uart.set_baud(baud);

        while (true) {
            std::string s = uart.read_some();
            if (!s.empty()) {
                std::cout << s << std::flush;
            }

            sleep_ms(50);
        }
    }

    if (sub == "send") {
        std::string device;
        int baud = 115200;
        std::string text;

        for (int i = 3; i < argc; ++i) {
    const std::string a = argv[i];
                if (a == "--device") {
                device = arg_value(i, argc, argv);
            } else if (a == "--baud") {
                baud = std::stoi(arg_value(i, argc, argv));
            } else if (a == "--text") {
                text = arg_value(i, argc, argv);
            } else {
                throw Error("Unknown uart send option: " + a);
            }
        }

        if (device.empty() || text.empty()) {
            throw Error("uart send requires --device and --text");
        }

        UartPort uart(device);
        uart.set_baud(baud);
        uart.send_text(text);

        std::cout << "OK\n";
        return;
    }

    throw Error("Unknown uart subcommand: " + sub);
}

void cmd_script(int argc, char** argv) {
    if (argc < 3) {
        throw Error("Usage: boardctl script run --path <script> -- <args...>");
    }

    const std::string sub = argv[2];

    if (sub != "run") {
        throw Error("Only 'script run' is implemented");
    }

    std::string path;
    std::vector<std::string> args;
    bool passthrough = false;

    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];

        if (passthrough) {
            args.push_back(a);
            continue;
        }

        if (a == "--path") {
            path = arg_value(i, argc, argv);
        } else if (a == "--") {
            passthrough = true;
        } else {
            throw Error("Unknown script option: " + a);
        }
    }

    if (path.empty()) {
        throw Error("script run requires --path");
    }

    const int rc = Process::run(path, args);
    std::cout << rc << "\n";
}

void cmd_usb(int argc, char** argv) {
    if (argc < 3) {
        throw Error("Usage: boardctl usb discover --vid <vid> --pid <pid>");
    }

    const std::string sub = argv[2];

    if (sub != "discover") {
        throw Error("Only 'usb discover' is implemented");
    }

    std::string vid;
    std::string pid;

    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];

        if (a == "--vid") {
            vid = arg_value(i, argc, argv);
        } else if (a == "--pid") {
            pid = arg_value(i, argc, argv);
        } else {
            throw Error("Unknown usb discover option: " + a);
        }
    }

    if (vid.empty() || pid.empty()) {
        throw Error("usb discover requires --vid and --pid");
    }

    std::string tty;
    const int found = UsbScanner::check_usb_device(vid, pid, true, tty);

    if (found > 0) {
        std::cout << tty << "\n";
    } else {
        std::cout << "not found\n";
    }
}

void cmd_gen5(int argc, char** argv) {
    if (argc < 4) {
        throw Error("Usage: boardctl gen5 power <on|off|reboot> --device /dev/ttyUSBx");
    }

    const std::string group = argv[2];
    const std::string action = argv[3];

    if (group != "power") {
        throw Error("Only 'gen5 power' is implemented");
    }

    std::string device;

    for (int i = 4; i < argc; ++i) {
        const std::string a = argv[i];

        if (a == "--device") {
            device = arg_value(i, argc, argv);
        } else {
            throw Error("Unknown gen5 power option: " + a);
        }
    }

    if (device.empty()) {
        throw Error("gen5 power requires --device");
    }

    UartPort power(device);
    power.set_baud(115200);

    if (action == "on") {
        power.send_text("POWER#ON\n");
    } else if (action == "off") {
        power.send_text("POWER#OFF\n");
    } else if (action == "reboot") {
        power.send_text("POWER#OFF\n");
        sleep_ms(2000);
        power.send_text("POWER#ON\n");
    } else {
        throw Error("Unknown gen5 power action: " + action);
    }

    std::cout << "OK\n";
}

void cmd_rtos(int argc, char** argv) {
    if (argc < 3) {
        throw Error("Usage: boardctl rtos capture --device <tty> --output <file>");
    }

    const std::string sub = argv[2];

    if (sub != "capture") {
        throw Error("Only 'rtos capture' is implemented");
    }

    RtosCaptureOptions opt;

    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];

        if (a == "--device") {
            opt.device = arg_value(i, argc, argv);
        } else if (a == "--baud") {
            opt.baud = std::stoi(arg_value(i, argc, argv));
        } else if (a == "--output") {
            opt.output = arg_value(i, argc, argv);
        } else if (a == "--seconds") {
            opt.seconds = std::stoi(arg_value(i, argc, argv));
        } else {
            throw Error("Unknown rtos capture option: " + a);
        }
    }

    if (opt.device.empty()) {
        throw Error("rtos capture requires --device");
    }

    if (opt.output.empty()) {
        throw Error("rtos capture requires --output");
    }

    RtosCapture capture;
    capture.capture(opt);

    std::cout << "OK\n";
}

void cmd_ipl(int argc, char** argv) {
    if (argc < 3) {
        throw Error("Usage: boardctl ipl <flash-gen4|flash-gen5> ...");
    }

    const std::string sub = argv[2];

    if (sub == "flash-gen4") {
        Gen4FlashOptions opt;

        for (int i = 3; i < argc; ++i) {
            const std::string a = argv[i];

            if (a == "--uart") {
                opt.uart = arg_value(i, argc, argv);
            } else if (a == "--relay-serial") {
                opt.relay_serial = arg_value(i, argc, argv);
            } else if (a == "--relay-channel") {
                opt.relay_channel =
                    static_cast<std::uint8_t>(std::stoi(arg_value(i, argc, argv)));
            } else if (a == "--gpio-chip") {
                opt.gpio_chip = arg_value(i, argc, argv);
            } else if (a == "--gpio-pins") {
                opt.gpio_pins = parse_pins(arg_value(i, argc, argv));
            } else if (a == "--path") {
                opt.image_dir = arg_value(i, argc, argv);
            } else if (a == "--profile") {
                opt.profile_path = arg_value(i, argc, argv);
            } else {
                throw Error("Unknown ipl flash-gen4 option: " + a);
            }
        }

        if (opt.uart.empty()) {
            throw Error("flash-gen4 requires --uart");
        }

        if (opt.relay_serial.empty()) {
            throw Error("flash-gen4 requires --relay-serial");
        }

        if (opt.gpio_pins.empty()) {
            throw Error("flash-gen4 requires --gpio-pins");
        }

        if (opt.image_dir.empty()) {
            throw Error("flash-gen4 requires --path");
        }

        Gen4Flasher flasher;
        flasher.flash(opt);

        std::cout << "OK\n";
        return;
    }

    if (sub == "flash-gen5") {
        Gen5IplOptions opt;

        for (int i = 3; i < argc; ++i) {
            const std::string a = argv[i];

            if (a == "--script") {
                opt.script = arg_value(i, argc, argv);
            } else if (a == "--sdk-version") {
                opt.sdk_version = arg_value(i, argc, argv);
            } else if (a == "--path") {
                opt.path = arg_value(i, argc, argv);
            } else if (a == "--power") {
                opt.power = arg_value(i, argc, argv);
            } else if (a == "--uart") {
                opt.uart = arg_value(i, argc, argv);
            } else {
                throw Error("Unknown ipl flash-gen5 option: " + a);
            }
        }

        if (opt.script.empty()) {
            throw Error("flash-gen5 requires --script");
        }

        if (opt.sdk_version.empty()) {
            throw Error("flash-gen5 requires --sdk-version");
        }

        if (opt.path.empty()) {
            throw Error("flash-gen5 requires --path");
        }

        if (opt.power.empty()) {
            throw Error("flash-gen5 requires --power");
        }

        if (opt.uart.empty()) {
            throw Error("flash-gen5 requires --uart");
        }

        Gen5IplRunner runner;
        const int rc = runner.run(opt);

        std::cout << rc << "\n";
        return;
    }

    throw Error("Unknown ipl subcommand: " + sub);
}

} // namespace

int Cli::run(int argc, char** argv) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    const std::string cmd = argv[1];

    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        print_help();
        return 0;
    }

    if (cmd == "relay") {
        cmd_relay(argc, argv);
    } else if (cmd == "gpio") {
        cmd_gpio(argc, argv);
    } else if (cmd == "uart") {
        cmd_uart(argc, argv);
    } else if (cmd == "usb") {
        cmd_usb(argc, argv);
    } else if (cmd == "ipl") {
        cmd_ipl(argc, argv);
    } else if (cmd == "gen5") {
        cmd_gen5(argc, argv);
    } else if (cmd == "script") {
        cmd_script(argc, argv);
    } else if (cmd == "rtos") {
        cmd_rtos(argc, argv);
    } else {
        throw Error("Unknown command: " + cmd);
    }

    return 0;
}

} // namespace boardctl