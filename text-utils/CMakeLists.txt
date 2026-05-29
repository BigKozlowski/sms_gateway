set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(text-utils
    src/text-utils.cpp
)

target_include_directories(text-utils
    PUBLIC include
)