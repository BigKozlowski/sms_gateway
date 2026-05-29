#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace TextUtils
{
    std::vector<std::string> splitLines(const std::string &s);

    std::string toHex(uint8_t v);
    uint8_t fromHexByte(const std::string &s);
    std::string bytesToHex(const std::vector<uint8_t> &data);

    bool isGsm7(char c);
    bool canEncodeGsm7(const std::string &s);

    std::vector<uint8_t> encodeGsm7(const std::string &text);
    std::string decodeGsm7(const std::vector<uint8_t> &data, size_t septetCount);

    std::string encodeUcs2(const std::string &utf8);
    std::string decodeUcs2(const std::string &hex);

    std::string swapDigits(const std::string &s);
    std::string encodePhone(const std::string &phone);
    std::string decodePhone(const std::string &semiOctets, size_t digits);

    std::vector<std::string> splitUtf8(const std::string &s, size_t maxCodepoints);
}