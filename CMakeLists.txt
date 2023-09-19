cmake_minimum_required(VERSION 3.27.0)
message(STATUS "Detected CMake version is: ${CMAKE_VERSION}")

project(gears)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++17 -Wall -Werror")

message(STATUS "Project file generation requested")
message(STATUS "Building from NDK at path: $ENV{ANDROID_NDK_HOME}")

add_executable( GEARS )

target_sources( GEARS PRIVATE
           src/entry.cpp 
           include/logger.h)

target_include_directories( GEARS PRIVATE "$ENV{ANDROID_NDK_HOME}/sources/android/" ) 
target_include_directories( GEARS PRIVATE "$ENV{ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/include/" )
target_include_directories( GEARS PRIVATE include/ )