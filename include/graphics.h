#pragma once
#include <android_native_app_glue.h>
#include <vulkan/vulkan.h>

namespace Gears
{
    class Graphics
    {
        public:

        Graphics(android_app* app);
        void EnumerateLayerProperties();
        void CreateSurface(ANativeWindow* nativeWindow, VkInstance& instance);

        private:

        [[maybe_unused]] android_app m_androidApp;
    };
}