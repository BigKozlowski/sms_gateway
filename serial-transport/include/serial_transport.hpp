#pragma once

#include <string>
#include <cstdint>
#include <vector>

class SerialTransport
{
public:
    SerialTransport(const std::string &device);
    ~SerialTransport();

    void openPort(int baudrate);
    void closePort();

    void writeBytes(const uint8_t *data, size_t size);
    void writeString(const std::string &s);

    int readBytes(uint8_t *buffer, size_t size, int timeoutMs);
    std::string readAvailable(int timeoutMs);

    int fd() const { return fd_; }

private:
    int fd_ = -1;
    std::string device_;
};