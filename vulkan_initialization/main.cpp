#define GLFW_INCLUDE_VULKAN
#include  <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <set>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char *> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

const std::vector<const char *> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
	const VkAllocationCallbacks *pAllocator,
	VkDebugUtilsMessengerEXT *pDebugMessenger )
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" );

	if ( func != nullptr )
	{
		return func( instance, pCreateInfo, pAllocator, pDebugMessenger );
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(
	VkInstance instance,
	VkDebugUtilsMessengerEXT debugMessenger,
	const VkAllocationCallbacks *pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" );
	
	if ( func != nullptr )
	{
		func( instance, debugMessenger, pAllocator );
	}
}

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete()
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

class HelloTriangleApplication
{
public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:

	GLFWwindow *window;
	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkSurfaceKHR surface;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice logicalDevice;
	VkQueue graphicsQueue;
	VkQueue presentQueue;

	void initWindow()
	{
		glfwInit();

		glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
		glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );
		
		window = glfwCreateWindow( WIDTH, HEIGHT, "Vulkan", nullptr, nullptr );
	}

	void initVulkan() 
	{
		createInstance();
		createSurface();
		setupDebugMessenger();
		pickPhysicalDevice();
		createLogicalDevice();
	}

	void createSurface()
	{
		if ( glfwCreateWindowSurface( instance, window, nullptr, &surface ) != VK_SUCCESS )
		{
			throw std::runtime_error( "failed to create window surface!" );
		}
	}

	void createLogicalDevice()
	{
		// To create a logical device we need to create a VkDeviceCreateInfo*
		// To create a VkDeviceCreateInfo we first need a VkDeviceQueueCreateInfo*
		
		// 1. Get QueueFamilyIndices to pass to queueFamilyIndex attribute
		QueueFamilyIndices indices = findQueueFamilies( physicalDevice );

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = {
			indices.graphicsFamily.value(),
			indices.presentFamily.value()
		};

		float queuePriority = 1.0f;
		for ( uint32_t queueFamily : uniqueQueueFamilies )
		{
			VkDeviceQueueCreateInfo queueCreateInfo = { };
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back( queueCreateInfo );
		}

		// 2. VkDeviceQueueCreateInfo
		VkDeviceQueueCreateInfo queueCreateInfo = { };
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
		queueCreateInfo.queueCount = 1;

		VkPhysicalDeviceFeatures deviceFeatures = { };

		VkDeviceCreateInfo deviceCreateInfo = { };
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());

		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();	

		if ( enableValidationLayers )
		{
			deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			deviceCreateInfo.enabledLayerCount = 0;
		}

		if ( vkCreateDevice( physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice ) != VK_SUCCESS )
		{
			throw std::runtime_error( "failed to create a logical device!" );
		}

		vkGetDeviceQueue( logicalDevice, indices.graphicsFamily.value(), 0, &graphicsQueue );
		vkGetDeviceQueue( logicalDevice, indices.presentFamily.value(), 0, &presentQueue );
	}

	void pickPhysicalDevice()
	{
		uint32_t physicalDevicesCount = 0;
		vkEnumeratePhysicalDevices(instance, &physicalDevicesCount, nullptr);

		// If the system does not have a GPU supporting Vulkan, we throw an error
		if ( physicalDevicesCount == 0 )
		{
			throw std::runtime_error( "failed finding GPUs with Vulkan Support" );
		}

		// Otherwise we populate a VkPhysicalDevice array with the available GPUs
		std::vector<VkPhysicalDevice> devices( physicalDevicesCount );
		vkEnumeratePhysicalDevices( instance, &physicalDevicesCount, devices.data() );
		
		for ( const auto &device : devices )
		{
			if ( isPhysicalDeviceSuitable( device ) )
			{
				physicalDevice = device;
				break;
			}
		}

		if ( physicalDevice == VK_NULL_HANDLE )
		{
			throw std::runtime_error( "failed to find a suitable GPU!" );
		}
	}

	void populateDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT &createInfo )
	{
		createInfo = { };
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = 
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;

	}

	void setupDebugMessenger()
	{
		if ( !enableValidationLayers )
		{
			return;
		}

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo( createInfo );

		if ( CreateDebugUtilsMessengerEXT( instance, &createInfo, nullptr, &debugMessenger ) != VK_SUCCESS )
		{
			throw std::runtime_error( "failed to set up debug messenger!" );
		}

	}

	void mainLoop()
	{
		while ( !glfwWindowShouldClose( window ) )
		{
			glfwPollEvents();
		}
	}

	void cleanup()
	{
		vkDestroySurfaceKHR( instance, surface, nullptr );

		vkDestroyDevice( logicalDevice, nullptr );

		if ( enableValidationLayers )
		{
			DestroyDebugUtilsMessengerEXT( instance, debugMessenger, nullptr );
		}

		vkDestroyInstance( instance, nullptr );

		glfwDestroyWindow( window );

		glfwTerminate();
	}
	
	void createInstance()
	{
		if ( enableValidationLayers && !checkValidationLayerSupport() )
		{
			throw std::runtime_error( "validation layers requested, but not available!" );
		}

		// 1. Pointer to struct with creation info
		// 2. Pointer to custom allocator callbacks, always nullptr in this sample
		// 3. Pointer to the variable that stores the handle to the new intance object

		// Create a VkApplicationInfo structure to pass to the VkInstanceCreateInfo pApplicationInfo
		VkApplicationInfo appInfo = { };
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Vulkan Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
		appInfo.pEngineName = "None";
		appInfo.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
		appInfo.apiVersion = VK_API_VERSION_1_0;

		// Create the VkInstanceCreateInfo structure.
		VkInstanceCreateInfo createInfo = { };
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		// You need an extension to interface with the Window System
		// this is neeeded because Vulkan is a platform independent API
		// GLFW provides a built-in function to query the extensions it 
		// needs to do that.
		/*
		* uint32_t glfwInstanceExtensionsCount;
		const char **glfwRequiredExtensions;
		glfwRequiredExtensions = glfwGetRequiredInstanceExtensions( &glfwInstanceExtensionsCount );
		createInfo.enabledExtensionCount = glfwInstanceExtensionsCount;
		createInfo.ppEnabledExtensionNames = glfwRequiredExtensions;
		*/
		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast <uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();
		

		// The following two members of the struct determine the global validation layers
		// to enable. If the variable enableValidationLayers is true or false
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		if ( enableValidationLayers )	// if true (Debug Mode)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t> (validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			populateDebugMessengerCreateInfo( debugCreateInfo );
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *) &debugCreateInfo;
		}
		else							// if false (Release Mode)
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		// 1. vkCreateInstance verifies that the requested layers exist. If not, it will
		//    return VK_ERROR_LAYER_NOT_PRESENT.
		// 2. vkCreateInstance verifies that the requested extensions are supported (e.g.
		//    in the implementation or in any enabled instance layer). If any requested
		//    if any requested extension is not supported, then vkCreateInstance will 
		//    return VK_ERROR_EXTENSION_NOT_PRESENT
		
		// To verify that the requested extensions are supported, we need to call:
		// vkEnumerateInstanceExtensionProperties(pLayerName, pPropertyCount, pProperties)
		//		
		//		pLayerName will be nullptr for this application
		//		pPropertyCount is a uint32_t points to an integer where number of properties will be stored
		//		pProperties is nullptr or a pointer to an array of VkExtensionProperties structures
		//

		uint32_t extensionPropertiesCount = 0;
		if ( vkEnumerateInstanceExtensionProperties(nullptr, &extensionPropertiesCount, nullptr ) != VK_SUCCESS )
		{
			throw std::runtime_error( "could not get extensionPropertiesCount." );
		}
		std::cout << "Number of Extension Properties Counted: " << extensionPropertiesCount << std::endl;
		
		// Allocate an array of VkExtensionProperties to hold the number of extensionPropertiesCount
		std::vector<VkExtensionProperties> extensionProperties( extensionPropertiesCount );

		// Now, we are going to query the extension properties details by calling vkEnumerate... again
		if ( vkEnumerateInstanceExtensionProperties( nullptr, &extensionPropertiesCount, extensionProperties.data() ) != VK_SUCCESS )
		{
			throw std::runtime_error( "coult not get extensionProperties.data" );
		}
		std::cout << "List of available extensions: " << std::endl;

		for ( const auto &extention : extensionProperties )
		{
			std::cout << "\t" << extention.extensionName << std::endl;
		}

		// Call the vkCreateInstance() function to createn a Vulkan instance object
		if ( vkCreateInstance( &createInfo, nullptr, &instance ) != VK_SUCCESS )
		{
			throw std::runtime_error( "failed to create instance!" );
		}
	}
	
	bool checkValidationLayerSupport()
	{
		uint32_t layerCount = 0;
		if ( vkEnumerateInstanceLayerProperties( &layerCount, nullptr ) != VK_SUCCESS )
		{
			throw std::runtime_error( "issues enumerating layerCount" );
		}

		std::vector<VkLayerProperties> availableLayerProperties( layerCount );

		if ( vkEnumerateInstanceLayerProperties( &layerCount, availableLayerProperties.data() ) != VK_SUCCESS )
		{
			throw std::runtime_error( "issues enumerating availableLayerProperties" );
		}

		for ( const char *layerName : validationLayers )
		{
			bool layerFound = false;

			for ( const auto &layerProperties : availableLayerProperties )
			{
				if ( strcmp( layerName, layerProperties.layerName ) == 0 )
				{
					layerFound = true;
					break;
				}
			}

			if ( !layerFound )
			{
				return false;
			}
		}

		return true;
	}
	
	bool checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties( physicalDevice, nullptr, &extensionCount, nullptr );

		std::vector<VkExtensionProperties> availableExtensions( extensionCount );
		vkEnumerateDeviceExtensionProperties( physicalDevice, nullptr, &extensionCount, availableExtensions.data() );

		std::set<std::string> requiredExtensions( deviceExtensions.begin(), deviceExtensions.end() );

		for ( const auto &extension : availableExtensions )
		{
			requiredExtensions.erase( extension.extensionName );
		}

		return requiredExtensions.empty();
	}

	bool isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice)
	{
		QueueFamilyIndices indices = findQueueFamilies( physicalDevice );

		bool extensionsSupported = checkDeviceExtensionSupport( physicalDevice );

		return indices.isComplete() && extensionsSupported;
	}

	/*
		This function checks which Queue Families are supported by device and which of them
		supports the commands that we want to use.
	*/
	QueueFamilyIndices findQueueFamilies( VkPhysicalDevice physicalDevice )
	{
		QueueFamilyIndices indices;
		uint32_t queueFamiliesCount = 0;
		
		vkGetPhysicalDeviceQueueFamilyProperties( physicalDevice, &queueFamiliesCount, nullptr );

		std::vector<VkQueueFamilyProperties> queueFamilies( queueFamiliesCount );
		vkGetPhysicalDeviceQueueFamilyProperties( physicalDevice, &queueFamiliesCount, queueFamilies.data() );

		int i = 0;
		for ( const auto &queueFamily : queueFamilies )
		{
			if ( queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT )
			{
				indices.graphicsFamily = i;
			}
			
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR( physicalDevice, i, surface, &presentSupport );
			if ( presentSupport )
			{
				indices.presentFamily = i;	
			}

			if ( indices.isComplete() )
			{
				break;
			}

			i++;
		}

		return indices;
	}

	std::vector<const char *> getRequiredExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		const char **glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );

		std::vector<const char *> extensions( glfwExtensions, glfwExtensions + glfwExtensionCount );

		if ( enableValidationLayers )	// if true
		{
			extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
		}

		return extensions;
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
		void *pUserData )
	{
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}
};

int main()
{
	HelloTriangleApplication app;

	try
	{
		app.run();
	} 
	catch (const std::exception& e )
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}