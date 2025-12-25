set(CMAKE_SYSTEM_NAME Generic)

set(CMAKE_C_COMPILER   armcc.exe)
set(CMAKE_CXX_COMPILER armcc.exe)
set(CMAKE_ASM_COMPILER armasm.exe)
set(CMAKE_LINKER       armlink.exe)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Toolchain / global compiler flags
set(CMAKE_C_FLAGS_INIT "--c99 --gnu -c --cpu Cortex-M0 -W -O2 -Ospace --apcs=interwork --split_sections -D__UVISION_VERSION=\"526\" --no-multibyte-chars")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_INIT}" CACHE STRING "C compiler flags" FORCE)

set(CMAKE_ASM_FLAGS_INIT "--cpu Cortex-M0 --apcs=interwork --pd \"__MICROLIB SETA 1\"")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS_INIT}" CACHE STRING "ASM compiler flags" FORCE)

unset(CMAKE_EXE_LINKER_FLAGS)
set(CMAKE_EXE_LINKER_FLAGS "--cpu Cortex-M0 --nodebug --strict --scatter ${SCATTER_FILE} 
--summary_stderr --info summarysizes --map  --info totals --info veneers 
-o core.axf")