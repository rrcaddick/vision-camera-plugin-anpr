cmake_minimum_required(VERSION 3.4.1)
project(VisionCameraPluginAnpr)

set (CMAKE_VERBOSE_MAKEFILE ON)
set (CMAKE_CXX_STANDARD 14)

add_library(vision-camera-plugin-anpr            SHARED
            ../cpp/vision-camera-plugin-anpr.cpp
            cpp-adapter.cpp
)

# Specifies a path to native header files.
include_directories(
            ../cpp
)
