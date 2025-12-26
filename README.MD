# Project Configuration Guide

This document explains **how to configure, build, and customize the project** in a clean and professional way.

It covers:

* Toolchain and SDK setup
* Device selection
* CMakePresets configuration
* LVGL integration and customization
* Device configuration headers
* Common pitfalls and validation steps

---

## 1. Prerequisites

Before configuring the project, make sure you have:

* CMake **>= 3.20**
* A supported C/C++ toolchain (ARMCC / GCC / Clang depending on device)
* Git (required for FetchContent / LVGL)

---

## 2. Environment Variables (Required)

The build system relies on **environment variables** to locate the SDK and toolchain.

Set the following variables **before running CMake**:

| Variable                 | Description                  |
| ------------------------ | -----------------------      |
| `DEVICE_CONFIG_FILE`     | Device config file (.cmake)  |
| `DEVICE_LVGL_CONFIG_FILE`| LVGL config file (.cmake)    |

### Example (Windows)

```bat
set DEVICE_CONFIG_FILE=C:/<vendor>/cmake/dev_cfg.cmake
set DEVICE_LVGL_CONFIG_FILE=C:/<vendor>/cmake/lvgl_cfg.cmake
```
---

### 2.1 `${DEVICE_CONFIG_FILE}`

Responsible for **build-system wiring**:

* SDK source files
* Include paths
* Compile definitions
* Linker flags

Example:

```cmake
set(DEVICE_PLATFORM_INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}
)

set(DEVICE_COMPILE_DEFINITIONS
    DEVICE_TRENDIT_T3
)
```

---

### 2.2 `${DEVICE_LVGL_CONFIG_FILE}`

Responsible for **build-system wiring for lvgl only**:

* LVGL build settings
* For more information read 5.3 section

> ✅ Numeric and structural device values **must live here**, not in CMake.

> ⚠️ These must be set **before** running CMake or using CMakePresets.

---

## 3. Device Selection (CMakePresets)

Each hardware target is represented as a **device preset**.

### `CMakePresets.json`

```json
{
  "name": "trendit_t3",
  "displayName": "Trendit T3 Device",
  "generator": "Ninja",
  "cacheVariables": {
    "DEVICE": "trendit_t3",
    "CMAKE_BUILD_TYPE": "Release"
  }
}
```

* `DEVICE` selects which device configuration is used
* The device name must match a folder in `devices/`

---

## 4. Device Configuration Structure

### 4.1 `device_config.h`

Describes **hardware capabilities** and is included by C code.
> ⚠️ You must create a `device_config.h` file and include it for the target.

Example:

```c
#pragma once

#define DEVICE_COLOR_DEPTH       16
#define DEVICE_DISPLAY_WIDTH     320
#define DEVICE_DISPLAY_HEIGHT    240

#define DEVICE_PRINTER_WIDTH_MM  58
#define DEVICE_PRINTER_DPI       203

#define DEVICE_HAS_TOUCH         1
#define DEVICE_HAS_ETHERNET      0
```

> ✅ Numeric and structural device values **must live here**, not in CMake.

---

## 5. LVGL Configuration

LVGL is integrated using **FetchContent**.

### 5.1 LVGL Fetch

LVGL is downloaded automatically during configuration:

```cmake
FetchContent_Declare(
  lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG release/v9.3
)

FetchContent_MakeAvailable(lvgl)
```

---

### 5.2 `lv_conf.h` Path

The LVGL configuration header **must be explicitly set**:

```cmake
set(LV_BUILD_CONF_PATH
    "${CMAKE_SOURCE_DIR}/include/lv_conf.h"
    CACHE STRING "LVGL configuration file"
    FORCE
)
```

CMake will fail if the file does not exist.

---

### 5.3 LVGL Environment Override (Important)

LVGL selects platform behavior via files in:

```
lvgl/env_support/cmake/
```

To customize this behavior, the project **copies a device-specific file** into LVGL:

```cmake
configure_file(
    ${CMAKE_SOURCE_DIR}/devices/${DEVICE}/lvgl_env_custom.cmake
    ${lvgl_SOURCE_DIR}/env_support/cmake/os_desktop.cmake
    COPYONLY
)
```

This allows:

* Custom compiler flags
* RTOS integration
* Platform-specific behavior

> LVGL automatically includes this file — no manual include required.

---

## 6. Application Sources

Application sources are collected from the project directory:

```cmake
file(GLOB_RECURSE APP_SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

add_executable(${PROJECT_NAME} ${APP_SRC})
```

Device SDK sources are added separately via `target_sources()`.

---

## 7. Post-Build Actions

Some devices require post-build steps (e.g. `.elf → .bin`).

Each device may define:

```cmake
set(DEVICE_POST_BUILD_COMMANDS
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:app> app.bin
)
```

The main project applies them automatically.

---

## 8. Build Instructions

### Configure

```bash
cmake --preset trendit_t3
```

### Build

```bash
cmake --build --preset trendit_t3
```

---

## 9. Debug & Validation Tips

* Print SDK source count:

```cmake
list(LENGTH DEVICE_SDK_SOURCES SDK_COUNT)
message(STATUS "SDK source files: ${SDK_COUNT}")
```

* Verify compilation:

```bash
cmake --build . --verbose
```

* Add temporary debug message in `lvgl_env_custom.cmake`:

```cmake
message(STATUS ">>> Using custom LVGL environment config")
```

---

## 10. Design Principles (Important)

* **CMake selects the device**
* **C headers describe the device**
* SDKs should be libraries, not injected source dumps
* Numeric hardware values belong in headers
* Build flags belong in CMake

This structure is inspired by **Zephyr, ESP-IDF, and professional embedded SDKs**.

---

## 11. Common Mistakes

❌ Putting hardware values only in CMake
❌ Using directories instead of source files
❌ Modifying LVGL upstream files manually
❌ Recursive include directories

---
