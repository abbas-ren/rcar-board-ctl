#pragma once

#include <filesystem>
#include <string>

namespace boardctl {

class UartPort {
public:
    UartPort() = default;
    explicit UartPort(const std::string& device);
    ~UartPort();

    UartPort(const UartPort&) = delete;
    UartPort& operator=(const UartPort&) = delete;

    UartPort(UartPort&& other) noexcept;
    UartPort& operator=(UartPort&& other) noexcept;

    void open_port(const std::string& device);
    void close_port();

    void set_baud(int baud);
    void flush();

    void send_text(const std::string& text);
    void send_file_lines(const std::filesystem::path& path);

    std::string read_some();
    bool wait_for(const std::string& expected, int timeout_sec);

    int fd() const { return fd_; }

private:
    int fd_ = -1;
};

}