#pragma once

#include <string>
#include <vector>
#include <cstdint>

class SmsPdu {
public:
    static std::string encodeUcs2(const std::string& text);
    static std::string decodeUcs2(const std::string& hex);

    static std::string encodePhone(const std::string& phone);
};