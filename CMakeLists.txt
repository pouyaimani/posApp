cmake_minimum_required(VERSION 3.20)

# -------------------------------------------------------------
# Verify that DEVICE was provided by CMakePresets.json
# -------------------------------------------------------------
if (NOT DEFINED DEVICE)
    message(FATAL_ERROR "DEVICE not set. Use cmake preset.")
endif()

if(NOT MCU_CORE)
    message(FATAL_ERROR "MCU_CORE not set. Use cmake preset.")
endif()

message(STATUS "  Building for device: ${DEVICE}")

# Normalize macro name
string(TOUPPER "${DEVICE}" DEVICE_NAME_UPPER)
set(DEVICE_DEFINE "DEVICE_${DEVICE_NAME_UPPER}")

# -------------------------------------------------------------
# Require Ninja or other generator from presets
# (Already handled by presets but we ensure silent failure)
# -------------------------------------------------------------
if(CMAKE_GENERATOR STREQUAL "")
    message(WARNING "It is recommended to use Ninja via presets.")
endif()


# -------------------------------------------------------------
# Optionally print all environment variables useful to device
# -------------------------------------------------------------
message(STATUS "SDK_ROOT = ${SDK_ROOT}")
message(STATUS "Preset build directory: ${CMAKE_BINARY_DIR}")


# -------------------------------------------------------------
# Project
# -------------------------------------------------------------

project(posapp LANGUAGES C)

add_subdirectory(src)

# -------------------------------------------------------------
# Device selection (each device has its own .cmake file)
# -------------------------------------------------------------
set(DEVICE_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/devices/${DEVICE}.cmake")

if(NOT EXISTS "${DEVICE_CONFIG_FILE}")
    message(FATAL_ERROR "Device configuration file not found: ${DEVICE_CONFIG_FILE}")
endif()

include(${DEVICE_CONFIG_FILE})   # <--- THIS loads all device paths, flags, sources, etc.


# -------------------------------------------------------------
# Global settings shared by all devices
# -------------------------------------------------------------

# Most embedded builds want compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        ${DEVICE_DEFINE}
        DEVICE_NAME_STR="${DEVICE_NAME}"
)

# -------------------------------------------------------------
# Device SDK library (from device config file)
# -------------------------------------------------------------
# Every device config file should define:
#
#   DEVICE_SDK_INCLUDE_DIRS
#   DEVICE_SDK_SOURCES
#   DEVICE_SDK_LIBRARIES
#   DEVICE_COMPILE_DEFINITIONS
#   DEVICE_C_FLAGS
#   DEVICE_ASM_FLAGS
#   DEVICE_LINK_FLAGS
#
# These come from presets → device.cmake
#
# -------------------------------------------------------------

target_include_directories(${PROJECT_NAME} PUBLIC
    ${DEVICE_SDK_INCLUDE_DIRS}
)

target_sources(${PROJECT_NAME} PRIVATE
    ${DEVICE_SDK_SOURCES}
)

target_compile_options(${PROJECT_NAME} PRIVATE 
    ${DEVICE_C_FLAGS}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ${DEVICE_COMPILE_DEFINITIONS}
)

set_property(TARGET ${PROJECT_NAME} PROPERTY ASM_FLAGS "--cpu=Cortex-M0 --apcs=interwork")

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${DEVICE_SDK_LIBRARIES}
)

target_link_options(${PROJECT_NAME} PRIVATE
    ${DEVICE_LINK_FLAGS}
)

# -------------------------------------------------------------
# 9. Post-build actions: convert .elf → .bin (device-specific)
# Device config must define a variable:
#     DEVICE_POST_BUILD_COMMANDS
# -------------------------------------------------------------
if(DEVICE_POST_BUILD_COMMANDS)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        ${DEVICE_POST_BUILD_COMMANDS}
    )
endif()


message(STATUS "----------------------------------------------------")
message(STATUS " Completed configuration for device: ${DEVICE}")
message(STATUS " Build directory: ${CMAKE_BINARY_DIR}")
message(STATUS "----------------------------------------------------")