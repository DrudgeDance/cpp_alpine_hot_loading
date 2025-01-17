cmake_minimum_required(VERSION 3.15)
project(your_app CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost REQUIRED COMPONENTS system)

add_executable(your_app src/main.cpp)
target_link_libraries(your_app PRIVATE 
    Boost::system
    Boost::headers
)

# Set C++17 standard
set_target_properties(your_app PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Enable static linking
set_target_properties(your_app PROPERTIES
    LINK_SEARCH_START_STATIC ON
    LINK_SEARCH_END_STATIC ON
)

target_compile_options(your_app PRIVATE
    -static
)

target_link_options(your_app PRIVATE
    -static
    -static-libgcc
    -static-libstdc++
) 