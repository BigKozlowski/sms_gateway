#pragma once

#include "serial_transport.hpp"
#include <string>

class AtChannel
{
public:
    explicit AtChannel(SerialTransport &transport);

    void execute(const std::string &cmd);

    void writeRaw(const std::string &data);

    void sendCmgs(int tpduLen);

    void sendEof(); // CTRL+Z

    void waitPrompt();

    std::string readResponse(int timeoutMs);

private:
    SerialTransport &t_;
    std::string buffer_;

    bool endsWithPrompt(const std::string &s);
    bool containsFinalResult(const std::string &s) const;
};