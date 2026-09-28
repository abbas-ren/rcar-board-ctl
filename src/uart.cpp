#include "uart.hpp"
#include "util.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <termios.h>
#include <unistd.h>

namespace boardctl {

namespace {

speed_t to_speed(int baud) {
    switch (baud) {
        case 921600: return B921600;
        case 115200: return B115200;
        case 57600:  return B57600;
        default:     return B115200;
    }
}

} // namespace

UartPort::UartPort(const std::string& device) {
    open_port(device);
}

UartPort::~UartPort() {
    close_port();
}

UartPort::UartPort(UartPort&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

UartPort& UartPort::operator=(UartPort&& other) noexcept {
    if (this != &other) {
        close_port();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

void UartPort::open_port(const std::string& device) {
    close_port();

    fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd_ < 0) {
        throw Error("Failed to open UART: " + device + ": " + std::strerror(errno));
    }

    termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        const std::string msg = std::strerror(errno);
        close_port();
        throw Error("tcgetattr failed: " + msg);
    }

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    cfsetospeed(&tty, B921600);
    cfsetispeed(&tty, B921600);

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        const std::string msg = std::strerror(errno);
        close_port();
        throw Error("tcsetattr failed: " + msg);
    }

    tcflush(fd_, TCIOFLUSH);
}

void UartPort::close_port() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

void UartPort::set_baud(int baud) {
    if (fd_ < 0) throw Error("UART not open");

    termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        throw Error("tcgetattr in set_baud failed");
    }

    const speed_t speed = to_speed(baud);
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        throw Error("tcsetattr in set_baud failed");
    }

    tcflush(fd_, TCIOFLUSH);
    sleep_ms(100);
}

void UartPort::flush() {
    if (fd_ < 0) throw Error("UART not open");
    tcflush(fd_, TCIOFLUSH);
}

void UartPort::send_text(const std::string& text) {
    if (fd_ < 0) throw Error("UART not open");
    ssize_t written = ::write(fd_, text.c_str(), text.size());
    if (written < 0) {
        throw Error("UART write failed");
    }
    fsync(fd_);
}

void UartPort::send_file_lines(const std::filesystem::path& path) {
    std::ifstream ifs(path);
    if (!ifs) {
        throw Error("Failed to open file for UART send: " + path.string());
    }

    std::string line;
    while (std::getline(ifs, line)) {
        line.push_back('\n');
        ssize_t written = ::write(fd_, line.c_str(), line.size());
        if (written < 0) {
            throw Error("UART write failed while sending file");
        }
        fsync(fd_);
        sleep_ms(10);
    }
}

std::string UartPort::read_some() {
    if (fd_ < 0) throw Error("UART not open");

    char buf[256];
    const int n = ::read(fd_, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        return std::string(buf);
    }
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        throw Error("UART read failed");
    }
    return "";
}

bool UartPort::wait_for(const std::string& expected, int timeout_sec) {
    std::string acc;
    const auto start = std::chrono::steady_clock::now();

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
        if (elapsed >= timeout_sec) {
            return false;
        }

        std::string chunk = read_some();
        if (!chunk.empty()) {
            acc += chunk;
            if (acc.find(expected) != std::string::npos) {
                return true;
            }
            if (acc.size() > 4096) {
                acc.erase(0, acc.size() - 2048);
            }
        }

        sleep_ms(100);
    }
}

}