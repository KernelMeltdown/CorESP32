# CorESP32 v7.0 - Root CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Project Info
set(PROJECT_VER "7.0")
set(PROJECT_NAME "CorESP32")

# ESP-IDF Setup
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Components (OHNE bootloader!)
set(EXTRA_COMPONENT_DIRS
    components/storage
    components/config
    components/hardware_adapter
    components/shell
    components/commands
    components/utils
)

project(${PROJECT_NAME} VERSION ${PROJECT_VER})

# Build Info
message(STATUS "======================================")
message(STATUS "  CorESP32 v${PROJECT_VER}")
message(STATUS "  Target: ${IDF_TARGET}")
message(STATUS "======================================")