cmake_minimum_required(VERSION 3.18.1)
message(STATUS "Detected CMake version is: ${CMAKE_VERSION}")

project(gears)

message(STATUS "Project file generation requested")
message(STATUS "Building from NDK at path: $ENV{ANDROID_NDK_HOME}")

add_executable( GEARS )

target_sources( GEARS PRIVATE
           src/entry.cpp 
           src/graphics.cpp
           include/pch.h
           include/graphics.h
           include/logger.h)

target_include_directories( GEARS PRIVATE "$ENV{ANDROID_NDK_HOME}/sources/android/native_app_glue/" )
target_include_directories( GEARS PRIVATE "$ENV{ANDROID_NDK_HOME}/sources/android/" ) 
target_include_directories( GEARS PRIVATE "$ENV{ANDROID_NDK_HOME}/sources/third_party/vulkan/src/include/" ) 
target_include_directories( GEARS PRIVATE "$ENV{ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/include/" )
target_include_directories( GEARS PRIVATE include/ )