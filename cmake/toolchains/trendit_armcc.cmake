set(CMAKE_SYSTEM_NAME Generic)

set(CMAKE_C_COMPILER   armcc.exe)
set(CMAKE_CXX_COMPILER armcc.exe)
set(CMAKE_ASM_COMPILER armasm.exe)
set(CMAKE_LINKER       armlink.exe)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Toolchain / global compiler flags
set(CMAKE_C_FLAGS_INIT "--c99 --cpu=Cortex-M0 -Ospace --apcs=interwork --split_sections --no-multibyte-chars --diag_suppress=177 --diag_suppress=1")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_INIT}" CACHE STRING "C compiler flags" FORCE)

set(CMAKE_CXX_FLAGS_INIT "--c99 --cpu=Cortex-M0 -Ospace --apcs=interwork --split_sections --no-multibyte-chars --diag_suppress=177 --diag_suppress=1")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS_INIT}" CACHE STRING "CXX compiler flags" FORCE)

set(CMAKE_ASM_FLAGS_INIT "--cpu=Cortex-M0 --apcs=interwork")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS_INIT}" CACHE STRING "ASM compiler flags" FORCE)
