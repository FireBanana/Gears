cmake_minimum_required(VERSION 3.27.0)
message(STATUS "Detected CMake version is: ${CMAKE_VERSION}")

# Backslash throws error in Windows
string(REPLACE "\\" "/" ANDROID_NDK_FMT ${ANDROID_NDK})

message(STATUS "APK generation requested")
message(STATUS "Building from NDK at path: ${ANDROID_NDK_FMT}")

# build native_app_glue as a static lib
set(${CMAKE_C_FLAGS}, "${CMAKE_C_FLAGS}")
# add_library(native_app_glue STATIC ${ANDROID_NDK_FMT}/sources/android/native_app_glue/android_native_app_glue.c)

# now build app's shared lib
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++17 -Wall -Werror")

# Export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
# set(CMAKE_SHARED_LINKER_FLAGS"${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

add_library(native-activity SHARED ../src/entry.cpp)

include_directories(native-activity ../include/)
include_directories(native-activity "C:/Program Files/RenderDocForMetaQuest/")
include_directories(native-activity "C:/Program Files/Unity/Hub/Editor/2021.3.15f1/Editor/Data/PluginAPI")

#target_include_directories(native-activity PRIVATE
#    ${ANDROID_NDK_FMT}/sources/android/native_app_glue)

target_link_directories(native-activity PRIVATE
 ${ANDROID_NDK_FMT}/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/aarch64-linux-android/33/)

#add_library(vk_validation SHARED IMPORTED)
#set_target_properties(vk_validation PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../libs/libVkLayer_khronos_validation.so")
#add_library(vk_validation_src SHARED IMPORTED)
#set_target_properties(vk_validation_src PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../libs/libc++_shared.so")

# add lib dependencies
target_link_libraries(native-activity
    android
    #native_app_glue
    EGL
    GLESv1_CM
    GLESv3
    dl
    #libvulkan.so
    #vk_validation
    #vk_validation_src
    log)
