#include "text-utils.hpp"

namespace TextUtils
{
    std::vector<std::string> splitLines(
        const std::string &s)
    {
        std::vector<std::string> out;

        std::stringstream ss(s);

        std::string line;

        while (std::getline(ss, line))
        {
            if (!line.empty() &&
                line.back() == '\r')
            {
                line.pop_back();
            }

            if (!line.empty())
                out.push_back(line);
        }

        return out;
    }

    std::string toHex(uint8_t v)
    {
        std::ostringstream ss;

        ss
            << std::uppercase
            << std::hex
            << std::setw(2)
            << std::setfill('0')
            << static_cast<int>(v);

        return ss.str();
    }

    uint8_t fromHexByte(
        const std::string &s)
    {
        return static_cast<uint8_t>(
            std::stoi(s, nullptr, 16));
    }

    std::string bytesToHex(
        const std::vector<uint8_t> &data)
    {
        std::string out;

        out.reserve(data.size() * 2);

        for (uint8_t b : data)
            out += toHex(b);

        return out;
    }

    bool isGsm7(char c)
    {
        static const std::string gsm =
            "@£$¥èéùìòÇ\nØø\rÅåΔ_ΦΓΛΩΠΨΣΘΞ"
            " !\"#¤%&'()*+,-./"
            "0123456789:;<=>?"
            "¡ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÑÜ§¿"
            "abcdefghijklmnopqrstuvwxyzäöñüà";

        return gsm.find(c) != std::string::npos;
    }

    bool canEncodeGsm7(
        const std::string &s)
    {
        for (char c : s)
        {
            if (!isGsm7(c))
                return false;
        }

        return true;
    }

    std::vector<uint8_t> encodeGsm7(
        const std::string &text)
    {
        std::vector<uint8_t> out;

        uint16_t accumulator = 0;
        int bits = 0;

        for (unsigned char c : text)
        {
            accumulator |= (c & 0x7F) << bits;

            bits += 7;

            while (bits >= 8)
            {
                out.push_back(
                    accumulator & 0xFF);

                accumulator >>= 8;
                bits -= 8;
            }
        }

        if (bits > 0)
        {
            out.push_back(
                accumulator & 0xFF);
        }

        return out;
    }

    std::string decodeGsm7(
        const std::vector<uint8_t> &data,
        size_t septetCount)
    {
        std::string out;

        uint32_t accumulator = 0;
        int bits = 0;

        size_t pos = 0;

        while (out.size() < septetCount)
        {
            while (bits < 7 && pos < data.size())
            {
                accumulator |= data[pos++] << bits;
                bits += 8;
            }

            out.push_back(
                accumulator & 0x7F);

            accumulator >>= 7;
            bits -= 7;
        }

        return out;
    }

    std::string encodeUcs2(
        const std::string &utf8)
    {
        std::string out;

        size_t i = 0;

        while (i < utf8.size())
        {
            uint32_t cp = 0;

            unsigned char c = utf8[i];

            if ((c & 0x80) == 0)
            {
                cp = c;
                i += 1;
            }
            else if ((c & 0xE0) == 0xC0)
            {
                cp =
                    ((utf8[i] & 0x1F) << 6) |
                    (utf8[i + 1] & 0x3F);

                i += 2;
            }
            else if ((c & 0xF0) == 0xE0)
            {
                cp =
                    ((utf8[i] & 0x0F) << 12) |
                    ((utf8[i + 1] & 0x3F) << 6) |
                    (utf8[i + 2] & 0x3F);

                i += 3;
            }
            else
            {
                /*
                    Unsupported UTF8 sequence
                */
                i += 1;
                continue;
            }

            out += toHex((cp >> 8) & 0xFF);
            out += toHex(cp & 0xFF);
        }

        return out;
    }

    std::string decodeUcs2(
        const std::string &hex)
    {
        std::string out;

        for (size_t i = 0; i + 3 < hex.size(); i += 4)
        {
            uint16_t cp = static_cast<uint16_t>(
                std::stoi(
                    hex.substr(i, 4),
                    nullptr,
                    16));

            if (cp < 0x80)
            {
                out.push_back(
                    static_cast<char>(cp));
            }
            else if (cp < 0x800)
            {
                out.push_back(
                    static_cast<char>(0xC0 | (cp >> 6)));

                out.push_back(
                    static_cast<char>(0x80 | (cp & 0x3F)));
            }
            else
            {
                out.push_back(
                    static_cast<char>(0xE0 | (cp >> 12)));

                out.push_back(
                    static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));

                out.push_back(
                    static_cast<char>(0x80 | (cp & 0x3F)));
            }
        }

        return out;
    }

    /*
        ==========================================================
        PHONE NUMBER ENCODING
        ==========================================================

        GSM semi-octet encoding.

        Example:

            +123456789

        becomes:

            21436587F9
    */

    std::string swapDigits(
        const std::string &s)
    {
        std::string out = s;

        if (out.size() % 2)
            out += 'F';

        for (size_t i = 0; i < out.size(); i += 2)
            std::swap(out[i], out[i + 1]);

        return out;
    }

    std::string encodePhone(
        const std::string &phone)
    {
        std::string num = phone;

        if (!num.empty() && num[0] == '+')
            num.erase(0, 1);

        return swapDigits(num);
    }

    std::string decodePhone(
        const std::string &semiOctets,
        size_t digits)
    {
        std::string tmp = semiOctets;

        for (size_t i = 0; i < tmp.size(); i += 2)
            std::swap(tmp[i], tmp[i + 1]);

        tmp.erase(
            std::remove(tmp.begin(), tmp.end(), 'F'),
            tmp.end());

        if (tmp.size() > digits)
            tmp.resize(digits);

        return "+" + tmp;
    }

    std::vector<std::string> splitUtf8(
        const std::string &s,
        size_t maxCodepoints)
    {
        std::vector<std::string> out;

        size_t i = 0;

        while (i < s.size())
        {
            size_t start = i;
            size_t count = 0;

            while (
                i < s.size() &&
                count < maxCodepoints)
            {
                unsigned char c = s[i];

                size_t len = 1;

                if ((c & 0x80) == 0x00)
                    len = 1;
                else if ((c & 0xE0) == 0xC0)
                    len = 2;
                else if ((c & 0xF0) == 0xE0)
                    len = 3;
                else if ((c & 0xF8) == 0xF0)
                    len = 4;

                i += len;
                count++;
            }

            out.push_back(
                s.substr(start, i - start));
        }

        return out;
    }

}
