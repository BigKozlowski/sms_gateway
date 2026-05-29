set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(gsm-modem
    src/gsm_modem.cpp
)

target_include_directories(gsm-modem
    PUBLIC include
)

target_link_libraries(gsm-modem
    PUBLIC at-channel
    PUBLIC sms-pdu
    PUBLIC text-utils
)