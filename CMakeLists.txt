cmake_minimum_required(VERSION 3.16)
project(sms_gateway CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(text-utils)
add_subdirectory(serial-transport)
add_subdirectory(at-channel)
add_subdirectory(sms-pdu)
add_subdirectory(gsm-modem)
add_subdirectory(sms-gateway)