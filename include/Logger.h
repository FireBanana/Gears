#pragma once

#include <android/log.h>
#include <vulkan/vulkan.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))

// TODO: Move to graphics?
inline VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(
    VkDebugReportFlagsEXT       flags,
    VkDebugReportObjectTypeEXT  objectType,
    uint64_t                    object,
    size_t                      location,
    int32_t                     messageCode,
    const char* pLayerPrefix,
    const char* pMessage,
    void* pUserData)
{
    LOGI("GearsError::VULKAN: %s", pMessage);
    return VK_FALSE;
}