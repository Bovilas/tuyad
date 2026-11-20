cmake_minimum_required(VERSION 3.16)

project(first_daemon_program VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The Tuya SDK must build shared libraries.
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)

# Tuya SDK is included in the project repository.
set(TUYA_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tuya-iot-core-sdk"
    CACHE PATH "Path to Tuya IoT Core SDK")

if(NOT EXISTS "${TUYA_SDK_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "Tuya SDK not found at ${TUYA_SDK_DIR}. "
        "Set TUYA_SDK_DIR to the SDK directory.")
endif()

# Build SDK targets only when required by our application.
add_subdirectory(
    "${TUYA_SDK_DIR}"
    "${CMAKE_BINARY_DIR}/tuya-sdk"
    EXCLUDE_FROM_ALL
)

add_executable(tuya_daemon
    src/main.c
    src/tuya_helper.c
    src/utils.c
)

target_include_directories(tuya_daemon PRIVATE
    "${TUYA_SDK_DIR}/examples/subdevice_basic_demo"
)

target_link_libraries(tuya_daemon PRIVATE
    link_core
    utils_modules
    platform_port
    middleware_implementation
)

target_compile_options(tuya_daemon PRIVATE
    -Wall
    -Wextra
)

set_target_properties(tuya_daemon PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
