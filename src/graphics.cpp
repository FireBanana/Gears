// Refactor to header

#include "graphics.h"
#include "Logger.h"

#include <vulkan/vulkan.h>
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

	LOGI("Extensions found: %d", count);
}

void Gears::Graphics::EnumerateDeviceExtensions()
{
	uint32_t count;
	VK_CALL(vkEnumerateDeviceExtensionProperties(m_PhysicalDevices[0], nullptr, &count, nullptr));
	auto extensions = std::vector<VkExtensionProperties>( count );
	VK_CALL(vkEnumerateDeviceExtensionProperties(m_PhysicalDevices[0], nullptr, &count, extensions.data()));

	LOGI("Device extensions found: ");

	for (auto& i : extensions)
	{
		m_DeviceExtensionNames.push_back(i.extensionName);
		LOGI("Extension Name: %s", i.extensionName);
	}
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
	int selectedQueueIndex = 0;

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

		++selectedQueueIndex;
	}

	if (selectedQueue == nullptr)
	{
		LOGI("No queue found with graphics bit");
		return {};
	}

	VkDeviceQueueCreateInfo queueInfo;
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueCount = selectedQueue->queueCount;
	queueInfo.queueFamilyIndex = selectedQueueIndex;

	return queueInfo;
}

void Gears::Graphics::CreateLogicalDevice(VkDeviceQueueCreateInfo queueCreateInfo)
{
	VkDeviceCreateInfo deviceInfo{};
	VkDeviceQueueCreateInfo queueInfo[] = {queueCreateInfo};

	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.enabledExtensionCount = m_DeviceExtensionNames.size();
	deviceInfo.ppEnabledExtensionNames = m_DeviceExtensionNames.data();
	deviceInfo.enabledLayerCount = static_cast<uint32_t>(m_LayerPropertyNames.size());
	deviceInfo.ppEnabledLayerNames = m_LayerPropertyNames.data();
	deviceInfo.pQueueCreateInfos = queueInfo;
	deviceInfo.queueCreateInfoCount = 1;

	VK_CALL(vkCreateDevice(m_PhysicalDevices[0], &deviceInfo, nullptr, &m_Device));
}

void Gears::Graphics::CreateInstance()
{
	m_LayerExtensionNames.push_back("VK_EXT_debug_report");

	static const char* layers[] = { "VK_LAYER_KHRONOS_validation" };

	VkInstanceCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	info.pNext = nullptr;
	info.enabledLayerCount = 1;
	info.ppEnabledLayerNames = layers;
	info.enabledExtensionCount = static_cast<uint32_t>( m_LayerExtensionNames.size() );
	info.ppEnabledExtensionNames = m_LayerExtensionNames.data();

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


//TODO: Iniitalize in APP_CMD_INIT_WINDOW
//void Graphics::CreateSurface(ANativeWindow* nativeWindow, VkInstance& instance)
//{
//    VkSurfaceKHR surface;

//    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
//    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
//    surfaceCreateInfo.window = nativeWindow;

//    if (vkCreateAndroidSurfaceKHR(instance, &surfaceCreateInfo, NULL, &surface) != VK_SUCCESS)
//    {
//        // Log: Error has occured.
//    }
//}