#include "serial_transport.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <stdexcept>
#include <cstring>
#include <iostream>

SerialTransport::SerialTransport(const std::string &device)
    : device_(device) {}

SerialTransport::~SerialTransport()
{
    closePort();
}

void SerialTransport::openPort(int baudrate)
{
    fd_ = ::open(device_.c_str(), O_RDWR | O_NOCTTY | O_SYNC);

    if (fd_ < 0)
        throw std::runtime_error("Failed to open serial port");

    termios tty{};
    if (tcgetattr(fd_, &tty) != 0)
        throw std::runtime_error("tcgetattr failed");

    cfmakeraw(&tty);

    speed_t br = B115200;

    switch (baudrate)
    {
        case 9600: br = B9600; break;
        case 19200: br = B19200; break;
        case 38400: br = B38400; break;
        case 57600: br = B57600; break;
        case 115200: br = B115200; break;
    }

    cfsetispeed(&tty, br);
    cfsetospeed(&tty, br);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    tcsetattr(fd_, TCSANOW, &tty);
}

void SerialTransport::closePort()
{
    if (fd_ >= 0)
    {
        ::close(fd_);
        fd_ = -1;
    }
}

void SerialTransport::writeBytes(const uint8_t *data, size_t size)
{
    if (fd_ < 0)
        throw std::runtime_error("serial not open");

    ::write(fd_, data, size);
}

void SerialTransport::writeString(const std::string &s)
{
    writeBytes(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

int SerialTransport::readBytes(uint8_t *buffer, size_t size, int timeoutMs)
{
    if (fd_ < 0)
        return -1;

    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd_, &set);

    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int rv = select(fd_ + 1, &set, nullptr, nullptr, &tv);

    if (rv <= 0)
        return 0;

    return ::read(fd_, buffer, size);
}

std::string SerialTransport::readAvailable(int timeoutMs)
{
    std::string out;
    uint8_t buf[256];

    int n = readBytes(buf, sizeof(buf), timeoutMs);

    if (n > 0) {
        out.append(reinterpret_cast<char*>(buf), n);
    }

    return out;
}