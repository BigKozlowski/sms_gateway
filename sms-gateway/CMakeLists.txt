set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(sms-gateway
    src/main.cpp
)

target_include_directories(sms-gateway
    PUBLIC include
)

target_link_libraries(sms-gateway
    PRIVATE gsm-modem
)