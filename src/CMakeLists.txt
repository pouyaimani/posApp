cmake_minimum_required(VERSION 3.20)

include(FetchContent)

file(GLOB_RECURSE APP_SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

add_executable(${PROJECT_NAME} ${APP_SRC})

# -------------------------------------------------------------
# Optional modules (like LVGL)
# -------------------------------------------------------------

# LVGL: Download and make available
# FetchContent_Declare(
#   lvgl
#   GIT_REPOSITORY https://github.com/lvgl/lvgl.git
#   GIT_TAG release/v9.3
# )

set(LV_BUILD_CONF_PATH
    "${CMAKE_SOURCE_DIR}/include/lv_conf.h"
    CACHE STRING "LVGL configuration file"
    FORCE
)

if(NOT EXISTS "${LV_BUILD_CONF_PATH}")
    message(FATAL_ERROR "lv_conf.h not found at ${LV_BUILD_CONF_PATH}")
endif()

set(CONFIG_LV_BUILD_EXAMPLES OFF)
set(CONFIG_LV_BUILD_DEMOS OFF)
set(LVGL_ROOT_DIR ${lvgl_SOURCE_DIR})
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(LV_USE_THORVG OFF CACHE BOOL "" FORCE)
set(LV_USE_VG_LITE OFF CACHE BOOL "" FORCE)
set(LV_USE_CPU_HELIUM OFF CACHE BOOL "" FORCE)
set(LV_USE_NATIVE_HELIUM_ASM 0 CACHE BOOL "" FORCE)
set(LV_USE_DRAW_SW_ASM 0 CACHE BOOL "" FORCE)
set(LVGL_ENABLE_CPP OFF CACHE BOOL "")
set(LVGL_ENABLE_ASM OFF CACHE BOOL "")


option(LVGL_ENABLE_CPP "Enable C++ for LVGL" OFF)
option(LVGL_ENABLE_ASM "Enable ASM for LVGL" OFF)

# FetchContent_MakeAvailable(lvgl)

target_include_directories(lvgl PUBLIC
    ${DEVICE_PLATFORM_INCLUDE_DIRS}
)

if(EXISTS $ENV{DEVICE_LVGL_CONFIG_FILE})
    message(STATUS "LVGL custom enviorment support file detected.")
    set(LVGL_ENV_CUSTOM_DST
        ${lvgl_SOURCE_DIR}/env_support/cmake/os_desktop.cmake
    )

    configure_file(
        $ENV{DEVICE_LVGL_CONFIG_FILE}
        ${LVGL_ENV_CUSTOM_DST}
        COPYONLY
    )
endif()


if(CMAKE_C_COMPILER_ID STREQUAL "ARMCC")
    # Remove Helium ASM from LVGL build
    list(REMOVE_ITEM LVGL_SOURCES
        ${LVGL_DIR}/src/draw/sw/blend/helium/lv_blend_helium.S
        # add any other .S files for GCC-only
    )
endif()

target_compile_definitions(lvgl PRIVATE
    D__MICROLIB
    LV_CONF_INCLUDE_SIMPLE
    LV_LVGL_H_INCLUDE_SIMPLE
    ${DEVICE_DEFINE}
)

target_link_libraries(${PROJECT_NAME} PRIVATE lvgl)

# -------------------------------------------------------------
# Device SDK library (from device config file)
# -------------------------------------------------------------
# Every device config file should define:
#
#   DEVICE_PLATFORM_INCLUDE_DIRS
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
    ${DEVICE_PLATFORM_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core/logger
)

target_sources(${PROJECT_NAME} PRIVATE
    ${DEVICE_SDK_SOURCES}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ${DEVICE_COMPILE_DEFINITIONS}
    ${DEVICE_DEFINE}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${DEVICE_SDK_LIBRARIES}
)

target_link_options(${PROJECT_NAME} PRIVATE
    ${DEVICE_LINK_FLAGS}
)

# -------------------------------------------------------------
# Post-build actions: convert .elf → .bin (device-specific)
# Device config must define a variable:
#     DEVICE_POST_BUILD_COMMANDS
# -------------------------------------------------------------
if(DEVICE_POST_BUILD_COMMANDS)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        ${DEVICE_POST_BUILD_COMMANDS}
    )
endif()