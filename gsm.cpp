#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstdint>
#include <algorithm>
#include <map>
#include <set>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>

#include "dotenv.h"

namespace TextUtils
{
    static inline std::vector<std::string> splitLines(
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

    /*
        ==========================================================
        HEX HELPERS
        ==========================================================
    */

    static inline std::string toHex(uint8_t v)
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

    static inline uint8_t fromHexByte(
        const std::string &s)
    {
        return static_cast<uint8_t>(
            std::stoi(s, nullptr, 16));
    }

    static inline std::string bytesToHex(
        const std::vector<uint8_t> &data)
    {
        std::string out;

        out.reserve(data.size() * 2);

        for (uint8_t b : data)
            out += toHex(b);

        return out;
    }

    /*
        ==========================================================
        GSM 03.38 BASIC TABLE
        ==========================================================

        NOTE:
        This is simplified.

        Production implementation should also support:

        - extension table
        - ESC sequences
        - national language shift tables
    */

    static inline bool isGsm7(char c)
    {
        static const std::string gsm =
            "@£$¥èéùìòÇ\nØø\rÅåΔ_ΦΓΛΩΠΨΣΘΞ"
            " !\"#¤%&'()*+,-./"
            "0123456789:;<=>?"
            "¡ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÑÜ§¿"
            "abcdefghijklmnopqrstuvwxyzäöñüà";

        return gsm.find(c) != std::string::npos;
    }

    static inline bool canEncodeGsm7(
        const std::string &s)
    {
        for (char c : s)
        {
            if (!isGsm7(c))
                return false;
        }

        return true;
    }

    /*
        ==========================================================
        GSM7 PACKING
        ==========================================================

        Packs:

            8 septets -> 7 octets

        IMPORTANT:

        This implementation is suitable for:

        - short GSM7 SMS

        Multipart GSM7 with UDH requires:

        - septet re-alignment
        - fill bits handling

        and SHOULD be implemented separately.
    */

    static inline std::vector<uint8_t> encodeGsm7(
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

    /*
        ==========================================================
        GSM7 UNPACKING
        ==========================================================
    */

    static inline std::string decodeGsm7(
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

    /*
        ==========================================================
        UTF8 -> UCS2 HEX
        ==========================================================

        IMPORTANT:

        Previous implementation was WRONG.

        It simply prepended 00 to UTF8 bytes.

        That breaks Cyrillic and all non-ASCII text.

        This implementation handles:

        - ASCII
        - 2-byte UTF8
        - 3-byte UTF8

        Production implementation should additionally support:

        - 4-byte UTF8
        - surrogate pairs
    */

    static inline std::string encodeUcs2(
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

    /*
        ==========================================================
        UCS2 HEX -> UTF8
        ==========================================================
    */

    static inline std::string decodeUcs2(
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

    static inline std::string swapDigits(
        const std::string &s)
    {
        std::string out = s;

        if (out.size() % 2)
            out += 'F';

        for (size_t i = 0; i < out.size(); i += 2)
            std::swap(out[i], out[i + 1]);

        return out;
    }

    static inline std::string encodePhone(
        const std::string &phone)
    {
        std::string num = phone;

        if (!num.empty() && num[0] == '+')
            num.erase(0, 1);

        return swapDigits(num);
    }

    static inline std::string decodePhone(
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

    static inline std::vector<std::string> splitUtf8(
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

class SerialPort
{
public:
    struct SmsMessage
    {
        std::string number;
        std::string text;
        std::string timestamp;

        bool multipart{false};

        uint8_t concatRef{0};
        uint8_t concatTotal{0};
        uint8_t concatSeq{0};
    };

public:
    SerialPort(
        const std::string &device,
        speed_t baud = B115200)
    {
        fd_ = open(
            device.c_str(),
            O_RDWR | O_NOCTTY);

        if (fd_ < 0)
        {
            throw std::runtime_error(
                std::string("open: ") + strerror(errno));
        }

        termios tty{};

        if (tcgetattr(fd_, &tty) != 0)
        {
            throw std::runtime_error(
                std::string("tcgetattr: ") + strerror(errno));
        }

        cfmakeraw(&tty);

        cfsetispeed(&tty, baud);
        cfsetospeed(&tty, baud);

        tty.c_cflag |= (CLOCAL | CREAD);

        tty.c_cflag &= ~CRTSCTS;
        tty.c_cflag &= ~PARENB;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;

        tty.c_cc[VMIN] = 1;
        tty.c_cc[VTIME] = 0;

        if (tcsetattr(fd_, TCSANOW, &tty) != 0)
        {
            throw std::runtime_error(
                std::string("tcsetattr: ") + strerror(errno));
        }

        tcflush(fd_, TCIOFLUSH);
    }

    ~SerialPort()
    {
        if (fd_ >= 0)
            close(fd_);
    }

public:
    void initialize()
    {
        command("AT");
        command("ATE0");
        command("AT+CMEE=2");

        /*
            PDU MODE
        */
        command("AT+CMGF=0");

        /*
            Realtime SMS notifications
        */
        command("AT+CNMI=2,1,0,2,0");
    }

public:
    std::string command(
        const std::string &cmd,
        int timeoutMs = 3000)
    {
        tcflush(fd_, TCIFLUSH);

        writeRaw(cmd + "\r");

        return readResponse(timeoutMs);
    }

    /*
        ==========================================================
        SMS SEND API
        ==========================================================
    */

public:
    void sendSms(
        const std::string &phone,
        const std::string &text)
    {
        /*
            Strategy:

            - short GSM7 -> GSM7
            - everything else -> UCS2 multipart

            This avoids GSM7 multipart alignment hell.
        */

        bool gsm7 =
            TextUtils::canEncodeGsm7(text);

        std::cout << "GSM7?: " << gsm7 << std::endl;
        if (gsm7 && text.size() <= 160)
        {
            sendSingleGsm7(phone, text);
            return;
        }

        sendMultipartUcs2(phone, text);
    }

public:
SmsMessage readSmsByIndex(
    int index
) {
    std::string resp =
        command(
            "AT+CMGR=" +
            std::to_string(index),
            1000
        );

    auto lines =
        TextUtils::splitLines(resp);

    for (size_t i = 0; i + 1 < lines.size(); ++i) {

        if (
            lines[i].rfind("+CMGR:", 0)
            == 0
        ) {
            return decodeDeliverPdu(
                lines[i + 1]
            );
        }
    }

    throw std::runtime_error(
        "SMS parse failed"
    );
}

public:
    std::vector<SmsMessage> readAllSmsReassembled()
    {
        auto raw =
            readAllSms();

        std::vector<SmsMessage> out;

        struct MultipartBuffer
        {
            SmsMessage base;
            std::map<uint8_t, std::string> parts;
        };

        std::map<
            std::string,
            MultipartBuffer>
            multipart;

        for (const auto &sms : raw)
        {

            if (!sms.multipart)
            {
                out.push_back(sms);
                continue;
            }

            std::string key =
                sms.number + "_" +
                std::to_string(sms.concatRef);

            auto &buf =
                multipart[key];

            buf.base = sms;

            buf.parts[sms.concatSeq] =
                sms.text;
        }

        for (auto &kv : multipart)
        {

            auto &buf = kv.second;

            if (
                buf.parts.size() !=
                buf.base.concatTotal)
            {
                /*
                    incomplete multipart
                */
                continue;
            }

            SmsMessage merged =
                buf.base;

            merged.text.clear();

            for (
                uint8_t i = 1;
                i <= buf.base.concatTotal;
                ++i)
            {
                merged.text +=
                    buf.parts[i];
            }

            out.push_back(merged);
        }

        return out;
    }

public:
    std::vector<SmsMessage> readAllSms()
    {
        std::vector<SmsMessage> out;

        /*
            PDU mode
        */
        std::string resp =
            command("AT+CMGL=4", 10000);

        auto lines = TextUtils::splitLines(resp);

        for (size_t i = 0; i + 1 < lines.size(); ++i)
        {
            const std::string &hdr = lines[i];

            if (hdr.rfind("+CMGL:", 0) != 0)
                continue;

            const std::string &pdu = lines[i + 1];

            try
            {
                SmsMessage sms =
                    decodeDeliverPdu(pdu);

                out.push_back(sms);
            }
            catch (...)
            {
                /*
                    ignore broken PDU
                */
            }

            ++i;
        }

        return out;
    }

private:
    void sendSingleGsm7(
        const std::string &phone,
        const std::string &text)
    {
        std::string pdu;

        /*
            SMSC info length
            00 -> use modem default SMSC
        */
        pdu += "00";

        /*
            SMS-SUBMIT
            VPF present
        */
        pdu += "11";

        /*
            Message reference
        */
        pdu += "00";

        std::string number = phone;

        if (!number.empty() && number[0] == '+')
            number.erase(0, 1);

        pdu += TextUtils::toHex(number.size());

        /*
            international number
        */
        pdu += "91";

        pdu += TextUtils::encodePhone(phone);

        /*
            PID
        */
        pdu += "00";

        /*
            DCS GSM7
        */
        pdu += "00";

        /*
            Validity period
        */
        pdu += "AA";

        auto packed =
            TextUtils::encodeGsm7(text);

        /*
            UDL = septet count
        */
        pdu += TextUtils::toHex(text.size());

        pdu += TextUtils::bytesToHex(packed);

        int tpduLength =
            (pdu.size() / 2) - 1;

        sendPdu(pdu, tpduLength);
    }

    void sendMultipartUcs2(
        const std::string &phone,
        const std::string &text)
    {
        /*
            UCS2 multipart limits:

            single SMS:
                70 chars

            multipart:
                67 chars
                (because UDH eats bytes)
        */

        constexpr size_t SINGLE_LIMIT = 70;
        constexpr size_t MULTI_LIMIT = 67;

        if (text.empty())
            return;

        /*
            Short UCS2 SMS
        */
        auto singleParts =
            TextUtils::splitUtf8(text, SINGLE_LIMIT);

        if (singleParts.size() == 1)
        {
            sendSingleUcs2(phone, text);
            return;
        }

        /*
            Multipart UCS2 SMS
        */
        auto parts =
            TextUtils::splitUtf8(
                text,
                MULTI_LIMIT);

        uint8_t ref = randomRef();

        for (size_t i = 0; i < parts.size(); ++i)
        {
            sendSingleUcs2(
                phone,
                parts[i],
                true,
                ref,
                static_cast<uint8_t>(parts.size()),
                static_cast<uint8_t>(i + 1));

            /*
                Small delay between parts.

                Some modems/network operators
                behave badly if parts are sent
                too fast.
            */
            std::this_thread::sleep_for(
                std::chrono::milliseconds(250));
        }
    }

private:
    uint8_t randomRef()
    {
        static std::mt19937 rng{
            std::random_device{}()};

        return static_cast<uint8_t>(rng() & 0xFF);
    }

private:
    void sendSingleUcs2(
        const std::string &phone,
        const std::string &text,
        bool multipart = false,
        uint8_t ref = 0,
        uint8_t total = 0,
        uint8_t seq = 0)
    {
        std::string pdu;

        /*
            SMSC
        */
        pdu += "00";

        /*
            SMS-SUBMIT

            single     -> 0x11
            multipart  -> 0x51
        */
        pdu += multipart ? "51" : "11";

        /*
            Message reference
        */
        pdu += "00";

        std::string number = phone;

        if (!number.empty() && number[0] == '+')
            number.erase(0, 1);

        pdu += TextUtils::toHex(number.size());

        pdu += "91";

        pdu += TextUtils::encodePhone(phone);

        /*
            PID
        */
        pdu += "00";

        /*
            UCS2
        */
        pdu += "08";

        /*
            VP
        */
        pdu += "AA";

        std::string udh;

        if (multipart)
        {
            /*
                05 = UDH length
                00 = concatenated SMS
                03 = IE length
            */

            udh += "050003";
            udh += TextUtils::toHex(ref);
            udh += TextUtils::toHex(total);
            udh += TextUtils::toHex(seq);
        }

        std::string payload =
            TextUtils::encodeUcs2(text);

        std::string userData =
            udh + payload;

        /*
            UDL = octet count
            for UCS2
        */
        pdu += TextUtils::toHex(
            userData.size() / 2);

        pdu += userData;

        int tpduLength =
            (pdu.size() / 2) - 1;

        sendPdu(pdu, tpduLength);
    }

    /*
        ==========================================================
        LOW LEVEL PDU SEND
        ==========================================================
    */

private:
    void sendPdu(
        const std::string &pdu,
        int tpduLength)
    {
        writeRaw(
            "AT+CMGS=" +
            std::to_string(tpduLength) +
            "\r");

        waitPrompt();

        writeRaw(pdu);

        char z = 26;

        ::write(fd_, &z, 1);

        std::string resp =
            readResponse(10000);

        if (resp.find("OK") == std::string::npos)
        {
            throw std::runtime_error(
                "CMGS failed:\n" + resp);
        }
    }

private:
    SmsMessage decodeDeliverPdu(
        const std::string &pdu)
    {
        SmsMessage sms;

        size_t pos = 0;

        /*
            SMSC
        */
        uint8_t smscLen =
            TextUtils::fromHexByte(
                pdu.substr(pos, 2));

        pos += 2;

        pos += smscLen * 2;

        /*
            First octet
        */
        uint8_t firstOctet =
            TextUtils::fromHexByte(
                pdu.substr(pos, 2));

        bool udhi =
            (firstOctet & 0x40) != 0;

        pos += 2;

        /*
            sender length
        */
        uint8_t addrLen =
            TextUtils::fromHexByte(
                pdu.substr(pos, 2));

        pos += 2;

        /*
            TOA
        */
        pos += 2;

        size_t semiLen =
            ((addrLen + 1) / 2) * 2;

        std::string semi =
            pdu.substr(pos, semiLen);

        pos += semiLen;

        sms.number =
            TextUtils::decodePhone(
                semi,
                addrLen);

        /*
            PID
        */
        pos += 2;

        /*
            DCS
        */
        uint8_t dcs =
            TextUtils::fromHexByte(
                pdu.substr(pos, 2));

        pos += 2;

        /*
            timestamp
        */
        sms.timestamp =
            pdu.substr(pos, 14);

        pos += 14;

        /*
            UDL
        */
        uint8_t udl =
            TextUtils::fromHexByte(
                pdu.substr(pos, 2));

        pos += 2;

        std::string ud =
            pdu.substr(pos);

        /*
            UCS2
        */
        if (dcs == 0x08)
        {

            if (udhi)
            {

                uint8_t udhl =
                    TextUtils::fromHexByte(
                        ud.substr(0, 2));

                /*
                    concatenated SMS
                */
                if (
                    ud.size() >= 12 &&
                    ud.substr(2, 2) == "00" &&
                    ud.substr(4, 2) == "03")
                {
                    sms.multipart = true;

                    sms.concatRef =
                        TextUtils::fromHexByte(
                            ud.substr(6, 2));

                    sms.concatTotal =
                        TextUtils::fromHexByte(
                            ud.substr(8, 2));

                    sms.concatSeq =
                        TextUtils::fromHexByte(
                            ud.substr(10, 2));
                }

                size_t skip =
                    (udhl + 1) * 2;

                ud = ud.substr(skip);
            }

            sms.text =
                TextUtils::decodeUcs2(ud);
        }
        else
        {
            /*
                simplified GSM7 decode
            */

            std::vector<uint8_t> bytes;

            for (
                size_t i = 0;
                i + 1 < ud.size();
                i += 2)
            {
                bytes.push_back(
                    TextUtils::fromHexByte(
                        ud.substr(i, 2)));
            }

            sms.text =
                TextUtils::decodeGsm7(
                    bytes,
                    udl);
        }

        return sms;
    }

    /*
        ==========================================================
        URC POLLING
        ==========================================================
    */

public:
    std::vector<std::string> pollUrc(
        int timeoutMs = 100)
    {
        std::vector<std::string> out;

        std::string data =
            readAvailable(timeoutMs);

        std::stringstream ss(data);

        std::string line;

        while (std::getline(ss, line))
        {
            if (!line.empty())
                out.push_back(line);
        }

        return out;
    }

    /*
        ==========================================================
        READ HELPERS
        ==========================================================
    */

private:
    void waitPrompt(
        int timeoutMs = 5000)
    {
        auto start =
            std::chrono::steady_clock::now();

        std::string data;

        while (true)
        {
            data += readAvailable(200);

            if (data.find('>') != std::string::npos)
                return;

            auto now =
                std::chrono::steady_clock::now();

            auto ms =
                std::chrono::duration_cast<
                    std::chrono::milliseconds>(now - start)
                    .count();

            if (ms >= timeoutMs)
                break;
        }

        throw std::runtime_error(
            "Prompt timeout");
    }

private:
    std::string readResponse(
        int timeoutMs)
    {
        auto start =
            std::chrono::steady_clock::now();

        std::string out;

        while (true)
        {
            out += readAvailable(200);

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

private:
    std::string readAvailable(
        int timeoutMs)
    {
        fd_set set;

        FD_ZERO(&set);
        FD_SET(fd_, &set);

        timeval tv{};

        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;

        int rv =
            select(
                fd_ + 1,
                &set,
                nullptr,
                nullptr,
                &tv);

        if (rv <= 0)
            return {};

        char buf[1024];

        ssize_t n =
            ::read(fd_, buf, sizeof(buf));

        if (n <= 0)
            return {};

        return std::string(buf, n);
    }

private:
    bool containsFinalResult(
        const std::string &s) const
    {
        return s.find("\r\nOK\r\n") != std::string::npos ||
               s.find("\r\nERROR\r\n") != std::string::npos ||
               s.find("+CMS ERROR") != std::string::npos ||
               s.find("+CME ERROR") != std::string::npos;
    }

private:
    void writeRaw(
        const std::string &s)
    {
        const char *p = s.data();

        size_t left = s.size();

        while (left > 0)
        {
            ssize_t n = ::write(fd_, p, left);

            if (n < 0)
            {
                if (errno == EINTR)
                    continue;

                throw std::runtime_error(
                    std::string("write: ") + strerror(errno));
            }

            left -= n;
            p += n;
        }
    }

private:
    int fd_{-1};
};

int main() {
    auto env = dotenv(".env");
    try {
        static const std::string TARGET_PHONE =
            env.get("TARGET_PHONE");

        SerialPort modem(env.get("DEVICE"));

        modem.initialize();

        std::cout
            << "SMS gateway started\n"
            << "Target: "
            << TARGET_PHONE
            << "\n\n";

        struct MultipartBuffer {
            SerialPort::SmsMessage base;

            std::map<
                uint8_t,
                std::string
            > parts;
        };

        std::map<
            std::string,
            MultipartBuffer
        > multipartCache;

        while (true) {

            /*
                Poll modem URC
            */
            auto urc =
                modem.pollUrc(50);

            for (const auto& line : urc) {
                if (
                    line.rfind("+CMTI:", 0)
                    != 0
                ) {
                    continue;
                }

                auto comma =
                    line.find(',');

                if (
                    comma ==
                    std::string::npos
                ) {
                    continue;
                }

                int index =
                    std::stoi(
                        line.substr(
                            comma + 1
                        )
                    );

                try {

                    auto sms =
                        modem.readSmsByIndex(
                            index
                        );

                    /*
                        Single-part SMS
                    */
                    if (!sms.multipart) {

                        std::cout
                            << "\n========== INCOMING ==========\n"
                            << "FROM: "
                            << sms.number
                            << "\n"
                            << "TEXT: "
                            << sms.text
                            << "\n"
                            << "==============================\n\n";

                        continue;
                    }

                    /*
                        Multipart SMS
                    */

                    std::cout << "sms.concatRef: " << sms.concatRef << std::endl;

                    std::string key =
                        sms.number + "_" +
                        std::to_string(
                            sms.concatRef
                        );

                    auto& buf =
                        multipartCache[key];

                    buf.base = sms;

                    buf.parts[
                        sms.concatSeq
                    ] = sms.text;

                    /*
                        Wait until all parts arrive
                    */
                    if (
                        buf.parts.size() !=
                        sms.concatTotal
                    ) {
                        continue;
                    }

                    /*
                        Reassemble
                    */
                    SerialPort::SmsMessage merged =
                        sms;

                    merged.text.clear();

                    for (
                        uint8_t i = 1;
                        i <= sms.concatTotal;
                        ++i
                    ) {
                        merged.text +=
                            buf.parts[i];
                    }

                    std::cout
                        << "\n========== INCOMING ==========\n"
                        << "FROM: "
                        << merged.number
                        << "\n"
                        << "TEXT: "
                        << merged.text
                        << "\n"
                        << "==============================\n\n";

                    multipartCache.erase(key);

                } catch (
                    const std::exception& e
                ) {

                    std::cerr
                        << "[READ ERROR] "
                        << e.what()
                        << "\n";
                }
            }

            fd_set set;

            FD_ZERO(&set);

            FD_SET(
                STDIN_FILENO,
                &set
            );

            timeval tv{};

            tv.tv_sec  = 0;
            tv.tv_usec = 0;

            int rv =
                select(
                    STDIN_FILENO + 1,
                    &set,
                    nullptr,
                    nullptr,
                    &tv
                );

            if (
                rv > 0 &&
                FD_ISSET(
                    STDIN_FILENO,
                    &set
                )
            ) {

                std::string text;

                std::getline(
                    std::cin,
                    text
                );

                if (text.empty())
                    continue;

                try {

                    modem.sendSms(
                        TARGET_PHONE,
                        text
                    );

                    std::cout
                        << "[SENT]\n";

                } catch (
                    const std::exception& e
                ) {

                    std::cerr
                        << "[SEND ERROR] "
                        << e.what()
                        << "\n";
                }
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(20)
            );
        }

    } catch (
        const std::exception& e
    ) {

        std::cerr
            << "Fatal error: "
            << e.what()
            << "\n";

        return 1;
    }
}