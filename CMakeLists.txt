cmake_minimum_required(VERSION 3.20)

include(FetchContent)

# -------------------------------------------------------------
# 1. Verify that DEVICE was provided by CMakePresets.json
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
# 2. Require Ninja or other generator from presets
# (Already handled by presets but we ensure silent failure)
# -------------------------------------------------------------
if(CMAKE_GENERATOR STREQUAL "")
    message(WARNING "It is recommended to use Ninja via presets.")
endif()


# -------------------------------------------------------------
# 3. Optionally print all environment variables useful to device
# -------------------------------------------------------------
message(STATUS "SDK_ROOT = ${SDK_ROOT}")
message(STATUS "Preset build directory: ${CMAKE_BINARY_DIR}")


# -------------------------------------------------------------
# 6. Application source code (device-independent)
# -------------------------------------------------------------

project(posapp LANGUAGES C)

file(GLOB_RECURSE APP_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_executable(${PROJECT_NAME} ${APP_SRC})


# -------------------------------------------------------------
# 4. Device selection (each device has its own .cmake file)
# -------------------------------------------------------------
set(DEVICE_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/devices/${DEVICE}.cmake")

if(NOT EXISTS "${DEVICE_CONFIG_FILE}")
    message(FATAL_ERROR "Device configuration file not found: ${DEVICE_CONFIG_FILE}")
endif()

include(${DEVICE_CONFIG_FILE})   # <--- THIS loads all device paths, flags, sources, etc.


# -------------------------------------------------------------
# 5. Global settings shared by all devices
# -------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Most embedded builds want compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        ${DEVICE_DEFINE}
        DEVICE_NAME_STR="${DEVICE_NAME}"
)

# -------------------------------------------------------------
# 7. Device SDK library (from device config file)
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
# 8. Optional modules (like LVGL)
# -------------------------------------------------------------

# LVGL: Download and make available
FetchContent_Declare(
  lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG release/v9.3
)

set(LV_BUILD_CONF_PATH
    "${CMAKE_SOURCE_DIR}/include/lv_conf.h"
    CACHE STRING "LVGL configuration file"
    FORCE
)

if(NOT EXISTS "${LV_BUILD_CONF_PATH}")
    message(FATAL_ERROR "lv_conf.h not found at ${LV_BUILD_CONF_PATH}")
endif()

FetchContent_MakeAvailable(lvgl)

if(CMAKE_C_COMPILER_ID STREQUAL "ARMCC")
    # Remove Helium ASM from LVGL build
    list(REMOVE_ITEM LVGL_SOURCES
        ${LVGL_DIR}/src/draw/sw/blend/helium/lv_blend_helium.S
        # add any other .S files for GCC-only
    )
endif()


set(LVGL_ROOT_DIR ${lvgl_SOURCE_DIR})
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(LV_USE_THORVG OFF CACHE BOOL "" FORCE)
set(LV_USE_VG_LITE OFF CACHE BOOL "" FORCE)
set(LV_USE_CPU_HELIUM OFF CACHE BOOL "" FORCE)
set(LV_USE_NATIVE_HELIUM_ASM 0 CACHE BOOL "" FORCE)
set(LV_USE_DRAW_SW_ASM 0 CACHE BOOL "" FORCE)

target_link_libraries(${PROJECT_NAME} PRIVATE lvgl)

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