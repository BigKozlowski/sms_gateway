#include "sms_pdu.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

static std::string toHex(uint8_t v) {
    std::ostringstream ss;
    ss << std::uppercase << std::hex
       << std::setw(2) << std::setfill('0')
       << (int)v;
    return ss.str();
}

std::string SmsPdu::encodePhone(const std::string& phone) {
    std::string num = phone;
    if (!num.empty() && num[0] == '+')
        num.erase(0,1);

    if (num.size() % 2) num += 'F';

    for (size_t i = 0; i < num.size(); i += 2)
        std::swap(num[i], num[i+1]);

    return num;
}

std::string SmsPdu::encodeUcs2(const std::string& utf8) {
    std::string out;

    for (size_t i = 0; i < utf8.size();) {
        uint32_t cp = 0;
        unsigned char c = utf8[i];

        if ((c & 0x80) == 0) {
            cp = c; i++;
        } else if ((c & 0xE0) == 0xC0) {
            cp = ((utf8[i]&0x1F)<<6) | (utf8[i+1]&0x3F);
            i += 2;
        } else {
            cp = ((utf8[i]&0x0F)<<12) |
                 ((utf8[i+1]&0x3F)<<6) |
                 (utf8[i+2]&0x3F);
            i += 3;
        }

        out += toHex((cp>>8)&0xFF);
        out += toHex(cp&0xFF);
    }

    return out;
}

std::string SmsPdu::decodeUcs2(const std::string& hex) {
    std::string out;

    for (size_t i = 0; i + 3 < hex.size(); i += 4) {
        uint16_t cp = std::stoi(hex.substr(i,4), nullptr, 16);

        if (cp < 0x80) out.push_back(cp);
        else if (cp < 0x800) {
            out.push_back(0xC0 | (cp>>6));
            out.push_back(0x80 | (cp&0x3F));
        } else {
            out.push_back(0xE0 | (cp>>12));
            out.push_back(0x80 | ((cp>>6)&0x3F));
            out.push_back(0x80 | (cp&0x3F));
        }
    }

    return out;
}