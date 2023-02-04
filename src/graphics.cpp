// Refactor to header

#include "graphics.h"
#include "Logger.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <vector>
#include <type_traits>

#define VK_CALL(x) if(x != VK_SUCCESS) { LOGI("GearsError::Vulkan error occured at line: %d", __LINE__); return; }

Gears::Graphics::Graphics( android_app* app ) :
	m_androidApp( *app )
{
	EnumerateLayerProperties();
	EnumerateLayerExtensions();
	CreateInstance();
	EnumeratePhysicalDevices();	
	EnumerateDeviceExtensions();
	SetupDebugCallbacks();

	auto queueInfo = SetupDeviceQueues();
	CreateLogicalDevice(queueInfo);
	CreateCommandBufferPool();
	CreateSurface(app->window);
	CachePhysicalDeviceCapabilities();
	CreateSwapChain();
}

void Gears::Graphics::EnumerateLayerProperties()
{
	uint32_t count;

	VK_CALL(vkEnumerateInstanceLayerProperties( &count, nullptr ));
	auto layers = std::vector<VkLayerProperties> ( count );
	VK_CALL(vkEnumerateInstanceLayerProperties( &count, layers.data() ));

	for (auto& i : layers)
	{
		m_LayerPropertyNames.push_back(i.layerName);
		LOGI("Layer Name: %s", i.layerName);
	}

	LOGI("Layers found: %d", count);
}

void Gears::Graphics::EnumerateLayerExtensions()
{
	uint32_t count;

	VK_CALL(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
	auto layers = std::vector<VkExtensionProperties> ( count );
	VK_CALL(vkEnumerateInstanceExtensionProperties(nullptr, &count, layers.data()));

	for (auto& i : layers)
	{
		m_LayerExtensionNames.push_back(i.extensionName);
		LOGI("Extension Name: %s", i.extensionName);
	}

	LOGI("Instance extensions found: %d", count);
}

void Gears::Graphics::EnumerateDeviceExtensions()
{
	uint32_t count;
	VK_CALL(vkEnumerateDeviceExtensionProperties(m_PhysicalDevices[0], nullptr, &count, nullptr));
	auto extensions = std::vector<VkExtensionProperties>( count );
	VK_CALL(vkEnumerateDeviceExtensionProperties(m_PhysicalDevices[0], nullptr, &count, extensions.data()));

	for (auto& i : extensions)
	{
		m_DeviceExtensionNames.push_back(i.extensionName);
		LOGI("Extension Name: %s", i.extensionName);
	}

	LOGI("Device extensions found: %d", count);
}

void Gears::Graphics::EnumeratePhysicalDevices()
{
	uint32_t count;

	VK_CALL(vkEnumeratePhysicalDevices(m_VkInstance, &count, nullptr));
	m_PhysicalDevices = std::vector<VkPhysicalDevice>(count);
	VK_CALL(vkEnumeratePhysicalDevices(m_VkInstance, &count, m_PhysicalDevices.data()));

	if (count == 0)
	{
		LOGI("No physical devices found on this system.");
		return;
	}

	// Cache device properties of main device
	vkGetPhysicalDeviceProperties(m_PhysicalDevices[0], &m_MainDeviceProperties);	

	LOGI("Physical devices statistics:");
	LOGI("Device Name: %s", m_MainDeviceProperties.deviceName);
	LOGI("Device Type: %u", m_MainDeviceProperties.deviceType);
	LOGI("Driver Version: %d", m_MainDeviceProperties.driverVersion);
}

VkDeviceQueueCreateInfo Gears::Graphics::SetupDeviceQueues()
{
	uint32_t count;
	const VkQueueFamilyProperties* selectedQueue = nullptr;
	m_SelectedGraphicQueueIndex = 0;

	vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevices[0], &count, nullptr);
	m_PhysicalQueueProperties = std::vector<VkQueueFamilyProperties>(count);
	vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevices[0], &count, m_PhysicalQueueProperties.data());
	
	LOGI("Device Queues found: %d", count);

	for (const auto& queue : m_PhysicalQueueProperties)
	{
		if ((queue.queueFlags >> VkQueueFlagBits::VK_QUEUE_GRAPHICS_BIT) & 0x1)
		{
			LOGI("Queue found with graphics bit.");
			selectedQueue = &queue;
			break;
		}

		++m_SelectedGraphicQueueIndex;
	}

	if (selectedQueue == nullptr)
	{
		LOGI("No queue found with graphics bit");
		return {};
	}

	static const float qPriorities[] = {1.0f};

	VkDeviceQueueCreateInfo queueInfo{};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueCount = selectedQueue->queueCount;
	queueInfo.queueFamilyIndex = m_SelectedGraphicQueueIndex;
	queueInfo.pQueuePriorities = qPriorities;

	return queueInfo;
}

void Gears::Graphics::CreateLogicalDevice(VkDeviceQueueCreateInfo queueCreateInfo)
{
	VkDeviceCreateInfo deviceInfo{};
	VkDeviceQueueCreateInfo queueInfo[] = {queueCreateInfo};

	static const char* extensions[] = { "VK_KHR_swapchain" };

	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.enabledExtensionCount = 1;
	deviceInfo.ppEnabledExtensionNames = extensions;
	deviceInfo.enabledLayerCount = 0;
	deviceInfo.ppEnabledLayerNames = nullptr;
	deviceInfo.pQueueCreateInfos = queueInfo;
	deviceInfo.queueCreateInfoCount = 1;

	VK_CALL(vkCreateDevice(m_PhysicalDevices[0], &deviceInfo, nullptr, &m_Device));
}

void Gears::Graphics::CreateCommandBufferPool()
{
	constexpr int COMMAND_BUFFER_SIZE = 1;

	VkCommandPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.queueFamilyIndex = m_SelectedGraphicQueueIndex;
	
	VK_CALL(vkCreateCommandPool(m_Device, &createInfo, nullptr, &m_CommandPool));

	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandBufferCount = COMMAND_BUFFER_SIZE;
	allocateInfo.commandPool = m_CommandPool;
	allocateInfo.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	std::vector<VkCommandBuffer> uCommandBufferList(COMMAND_BUFFER_SIZE);
	
	VK_CALL(vkAllocateCommandBuffers(m_Device, &allocateInfo, uCommandBufferList.data()));
	/* TODO: Test here... */
}

void Gears::Graphics::CreateSwapChain()
{
	VkSwapchainCreateInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface = m_Surface;
	info.minImageCount = m_SurfaceCapabilities.minImageCount;
	info.imageFormat = VkFormat::VK_FORMAT_R8G8B8A8_UINT;
	info.imageColorSpace = VkColorSpaceKHR::VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	info.imageExtent = m_SurfaceCapabilities.currentExtent;
	info.imageArrayLayers = 1; // 2 for Stereo Applications
	info.imageUsage = 
		VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | 
		VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	info.imageSharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
	info.clipped = VK_FALSE;

	VK_CALL(vkCreateSwapchainKHR(m_Device, &info, nullptr, &m_Swapchain));
}

void Gears::Graphics::CachePhysicalDeviceCapabilities()
{
	VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevices[0], m_Surface, &m_SurfaceCapabilities));
}

void Gears::Graphics::CreateInstance()
{
	static const char* extensions[] = { "VK_KHR_surface", "VK_KHR_android_surface", "VK_EXT_debug_report" };
	static const char* layers[]		= { "VK_LAYER_KHRONOS_validation" };

	VkInstanceCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	info.pNext = nullptr;
	info.enabledLayerCount = 1;
	info.ppEnabledLayerNames = layers;
	info.enabledExtensionCount = 3;
	info.ppEnabledExtensionNames = extensions;

	VK_CALL(vkCreateInstance(&info, nullptr, &m_VkInstance));
}

void Gears::Graphics::SetupDebugCallbacks()
{
	/* Load VK_EXT_debug_report entry points in debug builds */
	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT =
		reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>
		(vkGetInstanceProcAddr(m_VkInstance, "vkCreateDebugReportCallbackEXT"));

	VkDebugReportCallbackCreateInfoEXT callbackCreateInfo;
	callbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
	callbackCreateInfo.pNext = nullptr;
	callbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
		VK_DEBUG_REPORT_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
	callbackCreateInfo.pfnCallback = &DebugReportCallback;
	callbackCreateInfo.pUserData = nullptr;

	/* Register the callback */
	VkDebugReportCallbackEXT callback;
	VK_CALL(vkCreateDebugReportCallbackEXT(m_VkInstance, &callbackCreateInfo, nullptr, &callback));
}

void Gears::Graphics::CreateSurface(ANativeWindow* nativeWindow)
{
    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.window = nativeWindow;

	VK_CALL(vkCreateAndroidSurfaceKHR(m_VkInstance, &surfaceCreateInfo, NULL, &m_Surface));
}