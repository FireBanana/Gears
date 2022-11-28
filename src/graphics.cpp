// Refactor to header

#include <vulkan/vulkan.h>

#include "graphics.h"
#include "pch.h"
#include "Logger.h"

#define VK_CALL(x) if(x != VK_SUCCESS) { LOGI("GearsError::Vulkan error occured"); return; }

namespace Gears
{
    Graphics::Graphics(android_app* app) :
        m_androidApp(*app)
    {
        EnumerateLayerProperties();
    }

    void Graphics::EnumerateLayerProperties()
    {
        uint32_t propertyCount;
        VK_CALL(vkEnumerateInstanceLayerProperties(&propertyCount, nullptr));

        std::vector<VkLayerProperties> layerProperties(propertyCount);
        VK_CALL(vkEnumerateInstanceLayerProperties(&propertyCount, layerProperties.data()));

        LOGI("Layers found: %d", propertyCount);

        for (const auto& layer : layerProperties)
        {        
            LOGI("%s", layer.layerName);
        }
    }

    //TODO: Iniitalize in APP_CMD_INIT_WINDOW
    void Graphics::CreateSurface(ANativeWindow* nativeWindow, VkInstance& instance)
    {
        //VkSurfaceKHR surface;

        //VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
        //surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        //surfaceCreateInfo.window = nativeWindow;

        //if (vkCreateAndroidSurfaceKHR(instance, &surfaceCreateInfo, NULL, &surface) != VK_SUCCESS)
        //{
        //    // Log: Error has occured.
        //}
    }
};