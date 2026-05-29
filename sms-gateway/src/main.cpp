#include <iostream>
#include <thread>
#include "dotenv.h"

#include "serial_transport.hpp"
#include "at_channel.hpp"
#include "gsm_modem.hpp"

int main() {
    auto env = dotenv(".env");
    SerialTransport transport(env.get("DEVICE"));

    transport.openPort(115200);

    AtChannel at(transport);
    GsmModem modem(at);

    modem.initialize();

    std::cout << "Gateway started\n";

    while (true) {
        std::string text;
        std::getline(std::cin, text);

        if (!text.empty()) {
            bool ok = modem.sendSms(env.get("TARGET_PHONE"), text);
            std::cout << "SENT\n" << ok << std::endl;
        }
    }
}