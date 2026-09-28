# boardctl-cpp

C++ command-line utility for:

1. flashing IPL files over UART/USB
2. controlling relay channels
3. controlling Raspberry Pi GPIO pins
4. running test scripts
5. controlling Gen5 power TTY devices

## Build

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config libusb-1.0-0-dev libgpiod-dev
cmake -S . -B build
cmake --build build -j