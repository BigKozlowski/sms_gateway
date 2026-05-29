#pragma once

#include "at_channel.hpp"
#include "sms_pdu.hpp"
#include <string>

class GsmModem {
public:
    explicit GsmModem(AtChannel& at);

    void initialize();

    bool sendSms(const std::string& number, const std::string& text);

private:
    AtChannel& at_;
    void sendSingleGsm7(
        const std::string &phone,
        const std::string &text);
    void sendPdu(
        const std::string &pdu,
        int tpduLength);
    void sendMultipartUcs2(
        const std::string &phone,
        const std::string &text);
    void sendSingleUcs2(
        const std::string &phone,
        const std::string &text,
        bool multipart = false,
        uint8_t ref = 0,
        uint8_t total = 0,
        uint8_t seq = 0);
};