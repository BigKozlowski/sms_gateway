set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(serial-transport
    src/serial_transport.cpp
)

target_include_directories(serial-transport
    PUBLIC include
)

if (UNIX)
    target_link_libraries(serial-transport
        PUBLIC pthread
    )
endif()