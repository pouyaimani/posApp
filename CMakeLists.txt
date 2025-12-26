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
# Device selection (each device has its own .cmake file)
# -------------------------------------------------------------
if(NOT EXISTS "$ENV{DEVICE_CONFIG_FILE}")
    message(FATAL_ERROR "Device configuration file not found: ${DEVICE_CONFIG_FILE}")
endif()

include($ENV{DEVICE_CONFIG_FILE})   # <--- THIS loads all device paths, flags, sources, etc.


# -------------------------------------------------------------
# Project and CMake subdirectiries
# -------------------------------------------------------------

project(posapp LANGUAGES C)

add_subdirectory(src)


message(STATUS "----------------------------------------------------")
message(STATUS " Completed configuration for device: ${DEVICE}")
message(STATUS " Build directory: ${CMAKE_BINARY_DIR}")
message(STATUS "----------------------------------------------------")