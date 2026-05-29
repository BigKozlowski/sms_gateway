#include "gsm_modem.hpp"
#include "text-utils.hpp"
#include <iostream>
#include <random>
#include <thread>
#include <stdexcept>
#include <chrono>

uint8_t randomRef()
{
    static std::mt19937 rng{ std::random_device{}() };
    return static_cast<uint8_t>(rng() & 0xFF);
}

GsmModem::GsmModem(AtChannel &at)
    : at_(at) {}

void GsmModem::initialize()
{
    at_.execute("AT");
    at_.execute("ATE0");
    at_.execute("AT+CMEE=2");

    /*
        PDU MODE
    */
    at_.execute("AT+CMGF=0");

    /*
        Realtime SMS notifications
    */
    at_.execute("AT+CNMI=2,1,0,2,0");
}

bool GsmModem::sendSms(
    const std::string &phone,
    const std::string &text)
{
    bool gsm7 = TextUtils::canEncodeGsm7(text);

    std::cout << "GSM7?: " << gsm7 << std::endl;

    if (gsm7 && text.size() <= 160)
    {
        sendSingleGsm7(phone, text);
        return true;
    }

    sendMultipartUcs2(phone, text);
    return true;
}

void GsmModem::sendSingleGsm7(
    const std::string &phone,
    const std::string &text)
{
    std::string pdu;

    pdu += "00"; // SMSC
    pdu += "11"; // SMS-SUBMIT
    pdu += "00"; // MR

    std::string number = phone;
    if (!number.empty() && number[0] == '+')
        number.erase(0, 1);

    pdu += TextUtils::toHex(number.size());
    pdu += "91";
    pdu += TextUtils::encodePhone(number);

    pdu += "00"; // PID
    pdu += "00"; // DCS GSM7
    pdu += "AA"; // VP

    auto packed = TextUtils::encodeGsm7(text);

    // FIX: UD length must be packed size, not text.size()
    pdu += TextUtils::toHex(packed.size());

    pdu += TextUtils::bytesToHex(packed);

    int tpduLength = (pdu.size() / 2) - 1;

    sendPdu(pdu, tpduLength);
}

void GsmModem::sendPdu(
    const std::string &pdu,
    int tpduLength)
{
    at_.writeRaw(
        "AT+CMGS=" +
        std::to_string(tpduLength) +
        "\r");

    at_.waitPrompt();

    at_.writeRaw(pdu);
    at_.sendEof();

    std::string resp = at_.readResponse(10000);

    std::cout << "Resp: [" << resp << "]" << std::endl;

    if (resp.find("OK") == std::string::npos)
        throw std::runtime_error("CMGS failed:\n" + resp);
}

void GsmModem::sendSingleUcs2(
    const std::string &phone,
    const std::string &text,
    bool multipart,
    uint8_t ref,
    uint8_t total,
    uint8_t seq)
{
    std::string pdu;

    pdu += "00";
    pdu += multipart ? "51" : "11";
    pdu += "00";

    std::string number = phone;
    if (!number.empty() && number[0] == '+')
        number.erase(0, 1);

    pdu += TextUtils::toHex(number.size());
    pdu += "91";
    pdu += TextUtils::encodePhone(number);

    pdu += "00"; // PID
    pdu += "08"; // UCS2
    pdu += "AA"; // VP

    std::string udh;

    if (multipart)
    {
        udh += "050003";
        udh += TextUtils::toHex(ref);
        udh += TextUtils::toHex(total);
        udh += TextUtils::toHex(seq);
    }

    std::string payload = TextUtils::encodeUcs2(text);
    std::string userData = udh + payload;

    pdu += TextUtils::toHex(userData.size() / 2);
    pdu += userData;

    int tpduLength = (pdu.size() / 2) - 1;

    sendPdu(pdu, tpduLength);
}

void GsmModem::sendMultipartUcs2(
    const std::string &phone,
    const std::string &text)
{
    constexpr size_t SINGLE_LIMIT = 70;
    constexpr size_t MULTI_LIMIT = 67;

    if (text.empty())
        return;

    auto singleParts =
        TextUtils::splitUtf8(text, SINGLE_LIMIT);

    if (singleParts.size() == 1)
    {
        sendSingleUcs2(phone, text);
        return;
    }

    auto parts =
        TextUtils::splitUtf8(text, MULTI_LIMIT);

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

        std::this_thread::sleep_for(
            std::chrono::milliseconds(250));
    }
}