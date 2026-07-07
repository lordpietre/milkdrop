cmake_minimum_required(VERSION 3.20)
project(MilkDrop3 VERSION 3.33 LANGUAGES C CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Platform-specific setup
if(WIN32)
    set(PLATFORM_WIN32 TRUE)
    add_definitions(-DWIN32 -D_WINDOWS -D_CRT_SECURE_NO_WARNINGS -D_CRT_NON_CONFORMING_SWPRINTFS)
    add_definitions(-DUSE_VIS_HDR_HWND -DNSEEL_REENTRANT_EXECUTION -DSTRSAFE_NO_DEPRECATE)
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_LINUX TRUE)
    add_definitions(-DPLATFORM_LINUX -D_USE_MATH_DEFINES)
    find_package(OpenGL REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(GLEW REQUIRED glew)
    pkg_check_modules(GLFW3 REQUIRED glfw3)
    pkg_check_modules(PIPEWIRE libpipewire-0.3)
    pkg_check_modules(GLM glm)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/vis_milk2)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ns-eel2)

if(WIN32)
    # DirectX SDK paths (set DXSDK_DIR env var or provide via -D)
    if(DEFINED ENV{DXSDK_DIR})
        include_directories($ENV{DXSDK_DIR}/Include)
        link_directories($ENV{DXSDK_DIR}/Lib/x86)
    endif()
    add_definitions(-DUSE_VIS_HDR_HWND)
endif()

if(PLATFORM_LINUX)
    include_directories(${OPENGL_INCLUDE_DIRS})
    include_directories(${SDL2_INCLUDE_DIRS})
    include_directories(${GLEW_INCLUDE_DIRS})
    include_directories(${GLFW3_INCLUDE_DIRS})
    link_directories(${SDL2_LIBRARY_DIRS} ${GLEW_LIBRARY_DIRS} ${GLFW3_LIBRARY_DIRS})
    if(PIPEWIRE_FOUND)
        include_directories(${PIPEWIRE_INCLUDE_DIRS})
        add_definitions(-DHAVE_PIPEWIRE)
        link_directories(${PIPEWIRE_LIBRARY_DIRS})
    endif()
    if(GLM_FOUND)
        include_directories(${GLM_INCLUDE_DIRS})
    endif()
endif()

# Subdirectories
add_subdirectory(ns-eel2)
add_subdirectory(vis_milk2)
