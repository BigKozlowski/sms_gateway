set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(sms-pdu
    src/sms_pdu.cpp
)

target_include_directories(sms-pdu
    PUBLIC include
)