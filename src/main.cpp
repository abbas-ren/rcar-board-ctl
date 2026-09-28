#include "cli.hpp"
#include "util.hpp"

int main(int argc, char** argv) {
    try {
        boardctl::Cli cli;
        return cli.run(argc, argv);
    } catch (const std::exception& ex) {
        boardctl::log_error(ex.what());
        return 1;
    }
}