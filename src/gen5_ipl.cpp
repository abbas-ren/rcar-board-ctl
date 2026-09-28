#include "gen5_ipl.hpp"
#include "process.hpp"
#include "util.hpp"

namespace boardctl {

int Gen5IplRunner::run(const Gen5IplOptions& opt) {
    if (opt.script.empty()) {
        throw Error("Missing Gen5 script path");
    }

    if (opt.sdk_version.empty()) {
        throw Error("Missing Gen5 SDK version");
    }

    if (opt.path.empty()) {
        throw Error("Missing Gen5 IPL path");
    }

    if (opt.power.empty()) {
        throw Error("Missing Gen5 power TTY");
    }

    if (opt.uart.empty()) {
        throw Error("Missing Gen5 UART TTY");
    }

    const std::filesystem::path hil_path = opt.path / "HIL";

    std::vector<std::string> args{
        opt.sdk_version,
        hil_path.string(),
        opt.power,
        opt.uart
    };

    return Process::run(opt.script, args);
}

}