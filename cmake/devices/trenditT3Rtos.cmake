message(STATUS "Configuring Trendit T3 RTOS")

if(NOT DEFINED ENV{TRENDIT_SDK_PATH})
    message(FATAL_ERROR "You must set TRENDIT_SDK_PATH before including trendit.cmake")
endif()

set(TRENDIT_SDK_ROOT $ENV{TRENDIT_SDK_PATH})
set(TRENDIT_APP_ROOT $ENV{TRENDIT_PLATFORM_PATH}/app)
set(TRENDIT_LIB_SDK_ROOT $ENV{TRENDIT_SDK_LIB_PATH})

#-------------------------------
# Includes
#-------------------------------
set(DEVICE_PLATFORM_INCLUDE_DIRS 
    $ENV{TRENDIT_PLATFORM_PATH}
    ${TRENDIT_APP_ROOT}
    ${TRENDIT_APP_ROOT}/lib
    ${TRENDIT_APP_ROOT}/lib/dev
    ${TRENDIT_APP_ROOT}/lib/disp
    ${TRENDIT_APP_ROOT}/lib/trans
    ${TRENDIT_APP_ROOT}/lib/transmit
    ${TRENDIT_APP_ROOT}/lib/utils
    ${TRENDIT_APP_ROOT}/lv_i18n

    ${TRENDIT_LIB_SDK_ROOT}/inc

    ${TRENDIT_LIB_SDK_ROOT}/libsdkemv/emvlibsrc/dllemv/
    ${TRENDIT_LIB_SDK_ROOT}/libsdkemv/emvlibsrc/dllemvv20/inc/
    ${TRENDIT_LIB_SDK_ROOT}/libsdk/libsdkemv/emvlibsrc/dllemvv20/src/


    $ENV{TRENDIT_PLATFORM_PATH}/lv_port
    $ENV{TRENDIT_SDK_LIB_PATH}/inc
    $ENV{TRENDIT_PLATFORM_PATH}/ddi
)

set(DEVICE_SDK_SOURCE_DIRS 
    ${TRENDIT_APP_ROOT}
    ${TRENDIT_APP_ROOT}/lib/dev
    ${TRENDIT_APP_ROOT}/lib/disp
    ${TRENDIT_APP_ROOT}/lib/trans
    ${TRENDIT_APP_ROOT}/lib/transmit
    ${TRENDIT_APP_ROOT}/lib/utils
    ${TRENDIT_APP_ROOT}/lv_i18n
    $ENV{TRENDIT_PLATFORM_PATH}/lv_port
    $ENV{TRENDIT_PLATFORM_PATH}
)

set(DEVICE_SDK_SOURCES)

foreach(dir ${DEVICE_SDK_SOURCE_DIRS})
    file(GLOB_RECURSE SDK_SRC CONFIGURE_DEPENDS
        ${dir}/*.c
        ${dir}/*.S
    )
    list(APPEND DEVICE_SDK_SOURCES ${SDK_SRC})
endforeach()

#-------------------------------
# Compile definitions
#-------------------------------
set(DEVICE_COMPILE_DEFINITIONS 
    D__MICROLIB
    LV_CONF_INCLUDE_SIMPLE
    LV_LVGL_H_INCLUDE_SIMPLE
)


#-------------------------------
# Libraries
#-------------------------------
set(DEVICE_SDK_LIBRARIES 
    ${TRENDIT_SDK_ROOT}/libsdk1.0.18/libtrenditsdk.a
    ${TRENDIT_SDK_ROOT}/EMVL2.lib
)