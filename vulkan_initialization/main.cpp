#define GLFW_INCLUDE_VULKAN
#include  <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <optional>
#include <set>
#include <algorithm>
#include <fstream>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;
const int MAX_FRAMES_IN_FLIGHT = 2;

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

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

// -------------------------------------------------------------------------------------------------------------------------
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
	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	std::vector<VkImageView> swapChainImageViews;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	
	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	
	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	std::vector<VkFence> imagesInFlight;
	size_t currentFrame = 0;

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
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFrameBuffers();
		createCommandPool();
		createCommandBuffers();
		createSyncObjects();
	}
	void mainLoop()
	{
		while ( !glfwWindowShouldClose( window ) )
		{
			glfwPollEvents();
			drawFrame();
		}

		vkDeviceWaitIdle( logicalDevice );
	}

	void cleanup()
	{
		for ( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
		{
			vkDestroySemaphore( logicalDevice, renderFinishedSemaphores[i], nullptr );
			vkDestroySemaphore( logicalDevice, imageAvailableSemaphores[i], nullptr );
			vkDestroyFence( logicalDevice, inFlightFences[i], nullptr );
		}

		vkDestroyCommandPool( logicalDevice, commandPool, nullptr );

		for ( auto framebuffer : swapChainFramebuffers )
		{
			vkDestroyFramebuffer( logicalDevice, framebuffer, nullptr );
		}

		vkDestroyPipeline( logicalDevice, graphicsPipeline, nullptr );
		vkDestroyPipelineLayout( logicalDevice, pipelineLayout, nullptr );
		vkDestroyRenderPass( logicalDevice, renderPass, nullptr );

		for ( auto imageView : swapChainImageViews )
		{
			vkDestroyImageView( logicalDevice, imageView, nullptr );
		}

		vkDestroySwapchainKHR( logicalDevice, swapChain, nullptr );
		vkDestroyDevice( logicalDevice, nullptr );

		if ( enableValidationLayers )
		{
			DestroyDebugUtilsMessengerEXT( instance, debugMessenger, nullptr );
		}
		
		vkDestroySurfaceKHR( instance, surface, nullptr );
		vkDestroyInstance( instance, nullptr );

		glfwDestroyWindow( window );

		glfwTerminate();
	}
	
	void createSyncObjects()
	{
		imageAvailableSemaphores.resize( MAX_FRAMES_IN_FLIGHT );
		renderFinishedSemaphores.resize( MAX_FRAMES_IN_FLIGHT );
		inFlightFences.resize( MAX_FRAMES_IN_FLIGHT );
		imagesInFlight.resize( swapChainImages.size(), VK_NULL_HANDLE );

		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
		{
			if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
				 vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
				 vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
			{
				throw std::runtime_error( "failed to create synchronization objects for a frame!" );
			}
		}
	}
	
	void createCommandBuffers()
	{
		commandBuffers.resize( swapChainFramebuffers.size() );

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

		if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error( "failed to allocate command buffers!" );
		}

		for (size_t i = 0; i < commandBuffers.size(); i++ )
		{
			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = 0;
			beginInfo.pInheritanceInfo = nullptr;

			if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS )
			{
				throw std::runtime_error( " failed to begin recording command buffer!" );
			}

			// Starting a render pass
			VkRenderPassBeginInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = renderPass;
			renderPassInfo.framebuffer = swapChainFramebuffers[i];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = swapChainExtent;
			
			VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;

			vkCmdBeginRenderPass( commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );
			vkCmdBindPipeline( commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline );
			vkCmdDraw( commandBuffers[i], 3, 1, 0, 0 );
			vkCmdEndRenderPass( commandBuffers[i] );

			if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS )
			{
				throw std::runtime_error( "failed to record command buffer!" );
			}
		}
	}

	void createCommandPool()
	{
		QueueFamilyIndices queueFamilyIndices = findQueueFamilies( physicalDevice );

		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
		poolInfo.flags = 0;

		if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool) != VK_SUCCESS )
		{
			throw std::runtime_error( "failed to create command pool!" );
		}
	}
	
	void createFrameBuffers()
	{
		swapChainFramebuffers.resize( swapChainImageViews.size() );
		for (size_t i = 0; i < swapChainImageViews.size(); i++ )
		{
			VkImageView attachments[] = {
				swapChainImageViews[i]
			};

			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = swapChainExtent.width;
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS )
			{
				throw std::runtime_error( "failed to create framebuffer!" );
			}
		}
	}

	void createRenderPass()
	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS )
		{
			throw std::runtime_error( "failed to create render pass!" );
		}
	}

	void createGraphicsPipeline()
	{
		auto vertShaderCode = readFile( "shaders/vert.spv" );
		auto fragShaderCode = readFile( "shaders/frag.spv" );

		VkShaderModule vertShaderModule = createShaderModule( vertShaderCode );
		VkShaderModule fragShaderModule = createShaderModule( fragShaderCode );

		VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[ ] = { vertShaderStageInfo, fragShaderStageInfo };

		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.pVertexBindingDescriptions = nullptr;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexAttributeDescriptions = nullptr;

		// Input assembly
		// 
		// Describes what kind of geometry will be drawn from the vertices and whether
		// primitive restart should be enabled.
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
		inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.height = (float) swapChainExtent.height;
		viewport.width = (float) swapChainExtent.width;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { 0,0 };
		scissor.extent = swapChainExtent;

		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		// Rasterizer
		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.lineWidth = 1.0f;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;

		// Multisampling
		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;

		// Color blending
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		// Pipeline Layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pSetLayouts = nullptr;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;

		if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS )
		{
			throw std::runtime_error( "failed to create pipeline layout!" );
		}

		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = nullptr;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS )
		{
			throw std::runtime_error( "failed to create graphics pipeline!" );
		}
		
		// Destroy code
		vkDestroyShaderModule( logicalDevice, fragShaderModule, nullptr );
		vkDestroyShaderModule( logicalDevice, vertShaderModule, nullptr );
	}

	void createSwapChain()
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport( physicalDevice );

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat( swapChainSupport.formats );
		VkPresentModeKHR presentMode = chooseSwapPresentMode( swapChainSupport.presentModes );
		VkExtent2D extent = chooseSwapExtent( swapChainSupport.capabilities );

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if ( swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount )
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR swapchainCreateInfo = { };
		swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCreateInfo.surface = surface;

		// Specify the details of the Swap Chain Images
		swapchainCreateInfo.minImageCount = imageCount;
		swapchainCreateInfo.imageFormat = surfaceFormat.format;
		swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
		swapchainCreateInfo.imageExtent = extent;
		swapchainCreateInfo.imageArrayLayers = 1;
		swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = findQueueFamilies( physicalDevice );
		uint32_t queueFamilyIndices[ ] = {
			indices.graphicsFamily.value(),
			indices.presentFamily.value()
		};

		if ( indices.graphicsFamily != indices.presentFamily )
		{
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchainCreateInfo.queueFamilyIndexCount = 2;
			swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			swapchainCreateInfo.queueFamilyIndexCount = 0;
			swapchainCreateInfo.pQueueFamilyIndices = nullptr;
		}

		swapchainCreateInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainCreateInfo.presentMode = presentMode;
		swapchainCreateInfo.clipped = VK_TRUE;
		swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

		if ( vkCreateSwapchainKHR( logicalDevice, &swapchainCreateInfo, nullptr, &swapChain ) != VK_SUCCESS )
		{
			throw std::runtime_error( "failed to create swap chain!" );
		}
		
		vkGetSwapchainImagesKHR( logicalDevice, swapChain, &imageCount, nullptr );
		swapChainImages.resize( imageCount );
		vkGetSwapchainImagesKHR( logicalDevice, swapChain, &imageCount, swapChainImages.data() );

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;

	}

	void createImageViews()
	{
		swapChainImageViews.resize( swapChainImages.size() );

		for( size_t i = 0; i < swapChainImages.size(); i++ )
		{
			VkImageViewCreateInfo imageviewCreateInfo = {};
			imageviewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageviewCreateInfo.image = swapChainImages[i];
			imageviewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageviewCreateInfo.format = swapChainImageFormat;
			imageviewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageviewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageviewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageviewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			imageviewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageviewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageviewCreateInfo.subresourceRange.levelCount = 1;
			imageviewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageviewCreateInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(logicalDevice, &imageviewCreateInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS )
			{
				throw std::runtime_error( "failed to create image views!" );
			}
		}
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

	

	void drawFrame()
	{
		vkWaitForFences( logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX );

		uint32_t imageIndex;
		vkAcquireNextImageKHR( logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex );

		if (imagesInFlight[imageIndex] != VK_NULL_HANDLE )
		{
			vkWaitForFences( logicalDevice, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX );
		}

		imagesInFlight[imageIndex] = inFlightFences[currentFrame];

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[ ] = { imageAvailableSemaphores[currentFrame] };
		VkPipelineStageFlags waitStages[ ] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

		VkSemaphore signalSemaphores[ ] = { renderFinishedSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkResetFences( logicalDevice, 1, &inFlightFences[currentFrame] );

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS )
		{
			throw std::runtime_error( "failed to submit draw command buffer!" );
		}

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[ ] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = &imageIndex;

		vkQueuePresentKHR( presentQueue, &presentInfo );

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
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

		bool swapChainAdequate = false;
		if ( extensionsSupported )
		{
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport( physicalDevice );
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indices.isComplete() && extensionsSupported && swapChainAdequate;
	}

	VkSurfaceFormatKHR chooseSwapSurfaceFormat( const std::vector<VkSurfaceFormatKHR> &availableFormats )
	{
		for ( const auto &availableFormat : availableFormats )
		{
			if ( availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR )
			{
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	VkPresentModeKHR chooseSwapPresentMode( const std::vector<VkPresentModeKHR> &availablePresentModes )
	{
		for ( const auto &availablePresentMode : availablePresentModes )
		{
			if ( availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR )
			{
				return availablePresentMode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D chooseSwapExtent( const VkSurfaceCapabilitiesKHR &capabilities )
	{
		if ( capabilities.currentExtent.width != UINT32_MAX )
		{
			return capabilities.currentExtent;
		}
		else
		{
			VkExtent2D actualExtent = {
				WIDTH,
				HEIGHT
			};

			actualExtent.width =
				std::max( capabilities.minImageExtent.width,
						  std::min( capabilities.maxImageExtent.width,
									actualExtent.width ) );
			actualExtent.height =
				std::max( capabilities.minImageExtent.height,
						  std::min( capabilities.maxImageExtent.height,
									actualExtent.height ) );

			return actualExtent;
		}
	}

	VkShaderModule createShaderModule(const std::vector<char>& code)
	{
		// this function takes a buffer with the bytecode as parameter
		// and creates a VkShaderModule object from it
		VkShaderModuleCreateInfo shaderCreateInfo = {};
		shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderCreateInfo.codeSize = code.size();
		shaderCreateInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(logicalDevice, &shaderCreateInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error( "failed to create shader module!" );
		}

		return shaderModule;
	}

	/*
		Functions based on structs above
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

	SwapChainSupportDetails querySwapChainSupport( VkPhysicalDevice physicalDevice )
	{
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physicalDevice, surface, &details.capabilities );

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice, surface, &formatCount, nullptr );

		if ( formatCount != 0 )
		{
			details.formats.resize( formatCount );
			vkGetPhysicalDeviceSurfaceFormatsKHR( physicalDevice, surface, &formatCount, details.formats.data() );
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

		if ( presentModeCount != 0 )
		{
			details.presentModes.resize( presentModeCount );
			vkGetPhysicalDeviceSurfacePresentModesKHR( physicalDevice, surface, &presentModeCount, details.presentModes.data() );
		}


		return details;
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

	static std::vector<char> readFile(const std::string& filename)
	{
		std::ifstream file( filename, std::ios::ate | std::ios::binary );

		if (!file.is_open() )
		{
			throw std::runtime_error( "failed to open file!" );
		}

		size_t fileSize = (size_t) file.tellg();
		std::vector<char> buffer( fileSize );

		file.seekg( 0 );
		file.read( buffer.data(), fileSize );

		file.close();

		return buffer;
		
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