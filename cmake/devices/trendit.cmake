message(STATUS "Configuring Trendit T3 RTOS")

if(NOT DEFINED ENV{TRENDIT_SDK_PATH})
    message(FATAL_ERROR "You must set TRENDIT_SDK_PATH before including trendit.cmake")
endif()

set(TRENDIT_SDK_ROOT ENV{TRENDIT_SDK_PATH})

#-------------------------------
# Includes
#-------------------------------
# target_include_directories(${PROJECT_NAME} PRIVATE
#     ${CMAKE_SOURCE_DIR}/src/app
#     ${CMAKE_SOURCE_DIR}/src/app/lib
#     ${CMAKE_SOURCE_DIR}/src/app/lib/dev
#     ${CMAKE_SOURCE_DIR}/src/app/lib/disp
#     ${CMAKE_SOURCE_DIR}/src/app/lib/trans
#     ${CMAKE_SOURCE_DIR}/src/app/lib/transmit
#     ${CMAKE_SOURCE_DIR}/src/app/lib/utils
#     ${CMAKE_SOURCE_DIR}/src/app/lv_i18n

#     ${TREND_IT_SDK_PATH}/inc
#     ${TREND_IT_SDK_PATH}/src

#     ${LVGL_PATH}
#     ${CMAKE_SOURCE_DIR}/src/platform/rtos
#     ${CMAKE_SOURCE_DIR}/src/platform/rtos/lv_port
# )

#-------------------------------
# Sources
#-------------------------------
# file(GLOB_RECURSE TRENDIT_SOURCES
#     ${CMAKE_SOURCE_DIR}/src/app/*.c
#     ${CMAKE_SOURCE_DIR}/src/platform/rtos/*.c
# )

# target_sources(${PROJECT_NAME} PRIVATE ${TRENDIT_SOURCES})

#-------------------------------
# Compile definitions
#-------------------------------
target_compile_definitions(${PROJECT_NAME} PRIVATE
    __MICROLIB
    POS_PLATFORM_RTOS
    LV_CONF_INCLUDE_SIMPLE
    LV_LVGL_H_INCLUDE_SIMPLE
)

#-------------------------------
# Compiler flags
#-------------------------------
target_compile_options(${PROJECT_NAME} PRIVATE
    --c99
    --gnu
    --cpu Cortex-M0
    -Ospace
    --apcs=interwork
    --split_sections
    --no-multibyte-chars
)

#-------------------------------
# ASM flags
#-------------------------------
set_property(TARGET ${PROJECT_NAME} PROPERTY
    ASM_FLAGS "--cpu Cortex-M0 --apcs=interwork"
)

#-------------------------------
# Linker flags
#-------------------------------
target_link_options(${PROJECT_NAME} PRIVATE
    --cpu Cortex-M0
    --strict
    --scatter ${SCATTER_FILE}
)

#-------------------------------
# Libraries
#-------------------------------
# target_link_libraries(${PROJECT_NAME} PRIVATE
#     ${CMAKE_SOURCE_DIR}/src/platform/rtos/lib/EMVL2.lib
#     ${CMAKE_SOURCE_DIR}/src/platform/rtos/lib/libtrenditsdk.a
#     ${CMAKE_SOURCE_DIR}/src/platform/rtos/lib/liblvgl.a
# )
