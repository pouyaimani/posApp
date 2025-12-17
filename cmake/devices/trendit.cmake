message(STATUS "Configuring Trendit T3 RTOS")

if(NOT DEFINED ENV{TRENDIT_SDK_PATH})
    message(FATAL_ERROR "You must set TRENDIT_SDK_PATH before including trendit.cmake")
endif()

set(TRENDIT_SDK_ROOT ENV{TRENDIT_SDK_PATH})

#-------------------------------
# Includes
#-------------------------------
set(DEVICE_SDK_INCLUDE_DIRS ENV{TRENDIT_SDK_LIB_PATH}/inc)

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
set(DEVICE_COMPILE_DEFINITIONS 
    __MICROLIB
    POS_PLATFORM_RTOS
)

#-------------------------------
# Compiler flags
#-------------------------------

# set(DEVICE_C_FLAGS
#     --c99
#     --gnu
#     --cpu Cortex-M0
#     --Ospace
#     --split_sections
#     --no-multibyte-chars
#     --diag_suppress=177
#     --diag_suppress=1
# )

# set(DEVICE_ASM_FLAGS
#     --cpu Cortex-M0
# )

# set(CMAKE_C_FLAGS 
#     --c99
#     --gnu
#     --cpu Cortex-M0
#     --Ospace
#     --split_sections
#     --no-multibyte-chars
#     --diag_suppress=177
#     --diag_suppress=1
# )

# set(CMAKE_ASM_FLAGS
#     --cpu Cortex-M0
# )

#-------------------------------
# ASM flags
#-------------------------------
set_property(TARGET ${PROJECT_NAME} PROPERTY
    ASM_FLAGS "--cpu Cortex-M0 --apcs=interwork"
)

#-------------------------------
# Linker flags
#-------------------------------
set(DEVICE_LINK_FLAGS 
    --cpu Cortex-M0
    --strict
    --scatter ${SCATTER_FILE}
)

#-------------------------------
# Libraries
#-------------------------------
# target_link_libraries(${PROJECT_NAME} PRIVATE
#     ${TRENDIT_SDK_PATH}libsdk1.0.18/libtrenditsdk.a
# )
