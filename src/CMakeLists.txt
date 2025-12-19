cmake_minimum_required(VERSION 3.20)

include(FetchContent)

file(GLOB_RECURSE APP_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

add_executable(${PROJECT_NAME} ${APP_SRC})


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