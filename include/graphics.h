#pragma once

#include <vector>
#include <android_native_app_glue.h>
#include <vulkan/vulkan.h>
#include "Logger.h"

namespace Gears
{
    class Graphics
    {
        public:

        Graphics(android_app* app);

        private:

        [[maybe_unused]] android_app         m_androidApp;

        std::vector<const char*>             m_LayerPropertyNames;
        std::vector<const char*>             m_LayerExtensionNames;
        std::vector<const char*>             m_DeviceExtensionNames;
        std::vector<VkPhysicalDevice>        m_PhysicalDevices;
        std::vector<VkQueueFamilyProperties> m_PhysicalQueueProperties;

        VkInstance                           m_VkInstance;
        VkPhysicalDeviceProperties           m_MainDeviceProperties;
        VkDevice                             m_Device;
        VkCommandPool                        m_CommandPool;
        VkSurfaceKHR                         m_Surface;
        VkSurfaceCapabilitiesKHR             m_SurfaceCapabilities;
        VkSwapchainKHR                       m_Swapchain;

        uint32_t                             m_SelectedGraphicQueueIndex;
    
        void                    EnumerateLayerProperties();
        void                    CreateSurface(ANativeWindow* nativeWindow);
        void                    EnumerateLayerExtensions();
        void                    EnumerateDeviceExtensions();
        void                    EnumeratePhysicalDevices();
        void                    CreateInstance();
        void                    SetupDebugCallbacks();
        void                    CreateLogicalDevice(VkDeviceQueueCreateInfo queueCreateInfo);
        void                    CreateCommandBufferPool();
        void                    CreateSwapChain();
        void                    CachePhysicalDeviceCapabilities();
        VkDeviceQueueCreateInfo SetupDeviceQueues();
    };
}