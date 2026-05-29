#include "at_channel.hpp"
#include <thread>
#include <chrono>
#include <stdexcept>

AtChannel::AtChannel(SerialTransport &transport)
    : t_(transport) {}

void AtChannel::execute(const std::string &cmd)
{
    writeRaw(cmd + "\r");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    buffer_ += readResponse(500);
}

void AtChannel::writeRaw(const std::string &data)
{
    t_.writeString(data);
}

void AtChannel::sendCmgs(int tpduLen)
{
    writeRaw("AT+CMGS=" + std::to_string(tpduLen) + "\r");
}

void AtChannel::sendEof()
{
    uint8_t z = 26;
    t_.writeBytes(&z, 1);
}

bool AtChannel::endsWithPrompt(const std::string &s)
{
    return s.find(">") != std::string::npos;
}

void AtChannel::waitPrompt()
{
    std::string resp;

    for (int i = 0; i < 50; i++)
    {
        resp += readResponse(200);

        if (endsWithPrompt(resp))
            return;

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    throw std::runtime_error("No CMGS prompt");
}

std::string AtChannel::readResponse(
    int timeoutMs)
{
    auto start =
        std::chrono::steady_clock::now();

    std::string out;

    while (true)
    {
        out += t_.readAvailable(200);

        if (containsFinalResult(out))
            return out;

        auto now =
            std::chrono::steady_clock::now();

        auto ms =
            std::chrono::duration_cast<
                std::chrono::milliseconds>(now - start)
                .count();

        if (ms >= timeoutMs)
            break;
    }

    return out;
}

bool AtChannel::containsFinalResult(
    const std::string &s) const
{
    return s.find("\r\nOK\r\n") != std::string::npos ||
           s.find("\r\nERROR\r\n") != std::string::npos ||
           s.find("+CMS ERROR") != std::string::npos ||
           s.find("+CME ERROR") != std::string::npos;
}