set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(at-channel
    src/at_channel.cpp
)

target_include_directories(at-channel
    PUBLIC include
)

target_link_libraries(at-channel
    PUBLIC serial-transport
)