cmake_minimum_required(VERSION 3.15)
project(webserver VERSION 0.1.0)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Find dependencies
find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Add core library
add_library(webserver_core STATIC
    src/core/DynamicLoader.cpp
    src/core/FileMonitor.cpp
    src/core/PluginManager.cpp
)

target_include_directories(webserver_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(webserver_core PUBLIC
    Boost::boost
    OpenSSL::SSL
    OpenSSL::Crypto
    dl
)

# Add main executable - Note: removed static linking
add_executable(webserver src/main.cpp)

# Link with -rdynamic to export symbols for plugins
set_target_properties(webserver PROPERTIES
    ENABLE_EXPORTS ON
    LINK_FLAGS "-rdynamic"
)

target_link_libraries(webserver PRIVATE
    webserver_core
    Boost::boost
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)

# Add plugins
string(TIMESTAMP BUILD_TIMESTAMP "%Y%m%d_%H%M%S" UTC)

# Use build number from command line or default to 1
if(NOT DEFINED BUILD_NUMBER)
    set(BUILD_NUMBER 1)
endif()

# Set plugin name with timestamp to force reload
set(PLUGIN_NAME "hello_endpoint_${BUILD_TIMESTAMP}")
add_library(${PLUGIN_NAME} MODULE
    src/plugins/endpoints/HelloEndpoint.cpp
)

target_link_libraries(${PLUGIN_NAME} PRIVATE
    webserver_core
)

# Force recompilation of plugin
set_source_files_properties(
    src/plugins/endpoints/HelloEndpoint.cpp
    PROPERTIES 
    COMPILE_FLAGS "-O0 -fno-inline -fPIC"
    COMPILE_DEFINITIONS "BUILD_NUMBER=${BUILD_NUMBER};BUILD_TIMESTAMP=\"${BUILD_TIMESTAMP}\""
)

# Set plugin output directories with timestamp in name
set_target_properties(${PLUGIN_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/endpoints
    OUTPUT_NAME "hello_endpoint_${BUILD_TIMESTAMP}"
    PREFIX "lib"
    LINK_FLAGS "-Wl,--no-as-needed -Wl,-z,nodelete -rdynamic"
    VERSION "${BUILD_NUMBER}.0.0"
    SOVERSION "${BUILD_NUMBER}"
)

# Install targets
install(TARGETS webserver
    RUNTIME DESTINATION bin
)

install(TARGETS ${PLUGIN_NAME}
    LIBRARY DESTINATION bin/endpoints
)  