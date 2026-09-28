cmake_minimum_required(VERSION 3.16)
project(boardctl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiod)

add_executable(boardctl
    src/main.cpp
    src/cli.cpp
    src/relay.cpp
    src/gpio.cpp
    src/uart.cpp
    src/usb_scan.cpp
    src/process.cpp
    src/profile.cpp
    src/ipl_gen4.cpp
    src/gen5_ipl.cpp
    src/rtos.cpp
    src/util.cpp
)

target_include_directories(boardctl PRIVATE src)

target_link_libraries(boardctl PRIVATE
    PkgConfig::LIBUSB
    PkgConfig::GPIOD
)

target_compile_options(boardctl PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)