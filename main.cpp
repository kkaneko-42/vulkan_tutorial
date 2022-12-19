#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <limits>
#include <algorithm>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <cstdint>

const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation",
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger
) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(
	VkInstance instance,
	VkDebugUtilsMessengerEXT debugMessenger,
	const VkAllocationCallbacks* pAllocator
) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

class HelloTriangleApplication {
public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	GLFWwindow* _window;
	VkInstance _instance;
	// デバッグメッセージを自動出力する機能
	VkDebugUtilsMessengerEXT _debugMessenger;
	// 物理デバイス。接続されているハードと1対1対応
	VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
	// 論理デバイス。ここを通じてハードとやりとりする
	VkDevice _device;
	// コマンドキューのハンドル
	VkQueue _graphicsQueue;

	// ウィンドウサーフェス
	// ウィンドウシステムへのハンドル
	// 今回はGLFWのサーフェス
	// NOTE: オフスクリーンレンダリングの場合、当然これは不要
	VkSurfaceKHR _surface;

	// window surfaceへの表示コマンドキューへのハンドル
	VkQueue _presentQueue;

	VkSwapchainKHR _swapChain;
	// NOTE: imageの生成、破棄はswap chainが行う
	std::vector<VkImage> _swapChainImages;
	VkFormat _swapChainImageFormat;
	VkExtent2D _swapChainExtent;

	std::vector<VkImageView> _swapChainImageViews;

	VkRenderPass _renderPass;
	VkPipelineLayout _pipelineLayout;

	VkPipeline _graphicsPipeline;

	std::vector<VkFramebuffer> _swapChainFramebuffers;

	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool isComplete() {
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};

	// window surfaceとswap chainの互換性チェックの為
	struct SwapChainSupportDetails {
		// swap chain内の画像の最小・最大数、最小・最大幅と高さ、等
		VkSurfaceCapabilitiesKHR capabilities;
		// ピクセル形式、色空間
		std::vector<VkSurfaceFormatKHR> formats;
		// 利用可能なpresentation mode(?)
		std::vector<VkPresentModeKHR> presentModes;
	};

	void initWindow() {
		// GLFWライブラリの初期化
		glfwInit();

		// GLFWは元々OpenGL向け
		// OpenGLのコンテキストが必要ないことを伝える
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		// ウィンドウのリサイズはしない
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		// ウィンドウの取得
		_window = glfwCreateWindow(
			WIDTH,		/* 横幅 */
			HEIGHT,		/* 縦幅 */
			"Vulkan",	/* ウィンドウのタイトル */
			nullptr,	/* 表示するモニター */
			nullptr		/* OpenGLのみ関係 */
		);
	}

	void initVulkan() {
		createInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain(); // 論理デバイス作成後に呼ぶ
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
	}

	void createFramebuffers() {
		_swapChainFramebuffers.resize(_swapChainImageViews.size());

		for (std::size_t i = 0; i < _swapChainImageViews.size(); ++i) {
			VkImageView attachments[] = {
				_swapChainImageViews[i],
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = _renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = _swapChainExtent.width;
			framebufferInfo.height = _swapChainExtent.height;
			framebufferInfo.layers = 1;

			VkResult result = vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_swapChainFramebuffers[i]);
			if (result != VK_SUCCESS) {
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
	}

	void createGraphicsPipeline() {
		auto vertShaderCode = readFile("shaders/vert.spv");
		auto fragShaderCode = readFile("shaders/frag.spv");

		VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
		VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		// 当該shaderが、どのパイプラインステージで利用されるか
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		// 利用されるshader module
		vertShaderStageInfo.module = vertShaderModule;
		// entrypoint
		vertShaderStageInfo.pName = "main";
		// 他にも、pSpecializationInfoでシェーダー定数を設定できる

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		// 入力される頂点情報(今はvert shaderにhard codingしているので、特に指定なし)
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

		// 入力される図形情報
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		// viewport(画像 => frame buffer変換)
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)_swapChainExtent.width;
		viewport.height = (float)_swapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// frame bufferのトリミング
		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = _swapChainExtent;

		// パイプラインを作り直さなくても指定できる状態(dynamic state)の指定
		// NOTE: viewportとscissorを選択すると便利
		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
		};

		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		// NOTE: 実際のviewportとscissorは、描画時に(つまりdynamicに)設定される
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;
		/* viewportとscissorがdynamic stateにない場合はこれらも設定
		viewportState.pViewports = &viewport;
		viewportState.pScissors = &scissor;
		*/

		// 図形 → ピクセルの変換
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // MODE_LINE: 辺描画, MODEL_POINT: 頂点描画
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f; // Optional
		rasterizer.depthBiasClamp = 0.0f; // Optional
		rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f; // Optional
		multisampling.pSampleMask = nullptr; // Optional
		multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
		multisampling.alphaToOneEnable = VK_FALSE; // Optional

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f; // Optional
		colorBlending.blendConstants[1] = 0.0f; // Optional
		colorBlending.blendConstants[2] = 0.0f; // Optional
		colorBlending.blendConstants[3] = 0.0f; // Optional

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0; // Optional
		pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
		pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
		pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

		VkResult result = vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr; // Optional
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = _pipelineLayout;
		pipelineInfo.renderPass = _renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional

		result = vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphics pipeline!");
		}

		vkDestroyShaderModule(_device, vertShaderModule, nullptr);
		vkDestroyShaderModule(_device, fragShaderModule, nullptr);
	}

	void createRenderPass() {
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = _swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		if (vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_renderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render pass!");
		}
	}

	VkShaderModule createShaderModule(const std::vector<char>& code) {
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule;
		VkResult result = vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module!");
		}

		return shaderModule;
	}

	void createImageViews() {
		_swapChainImageViews.resize(_swapChainImages.size());
		for (std::size_t i = 0; i < _swapChainImages.size(); ++i) {
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = _swapChainImages[i];

			// 画像データをどのように解釈するか(2Dテクスチャ、3Dテクスチャ等)
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = _swapChainImageFormat;

			// FROM tutorial: swizzle...メソッドの機能を動的に変更する
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			VkResult result = vkCreateImageView(_device, &createInfo, nullptr, &_swapChainImageViews[i]);
			if (result != VK_SUCCESS) {
				throw std::runtime_error("failed to create image views!");
			}
		}
	}

	// NOTE: 論理デバイス作成後に呼び出されることを期待
	void createSwapChain() {
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(_physicalDevice);

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		// swap chainに含める画像の数
		// NOTE: ドライバ内部処理の完了待機を避ける為、minImageCountよりも多い画像を含める
		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

		// 画像の数に上限がある時
		// NOTE: 上限がない時、maxImageCount == 0
		if (swapChainSupport.capabilities.maxImageCount > 0) {
			// 画像の数が最大値を超えないよう調整
			if (imageCount > swapChainSupport.capabilities.maxImageCount) {
				imageCount = swapChainSupport.capabilities.maxImageCount;
			}
		}

		// create Infoの作成
		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType			= VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface			= _surface;
		createInfo.minImageCount	= imageCount;
		createInfo.imageFormat		= surfaceFormat.format;
		createInfo.imageColorSpace  = surfaceFormat.colorSpace;
		createInfo.imageExtent		= extent;
		createInfo.imageArrayLayers = 1; // 各画像を構成するレイヤーの数。stereoscopic 3Dアプリでない限り、1となる
		createInfo.imageUsage		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // swap chain内の画像への操作方法

		QueueFamilyIndices indices = findQueueFamilies(_physicalDevice);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		if (indices.graphicsFamily != indices.presentFamily) {
			// 画像は、複数のキューファミリで使用できる
			createInfo.imageSharingMode		 = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices   = queueFamilyIndices;
		}
		else {
			// 画像は、1度に1つのキューファミリに所有される。パフォーマンス良し。
			// 別のキューファミリに使用される際、所有権の明示的譲渡が必要
			createInfo.imageSharingMode		 = VK_SHARING_MODE_EXCLUSIVE;

			// 以下は複数のキューファミリ用なので、ここでは必要なし
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices   = nullptr;
		}

		createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode	  = presentMode;
		createInfo.clipped		  = VK_TRUE; // 別windowで隠れている等で見えていないピクセルを考慮しない
		createInfo.oldSwapchain   = VK_NULL_HANDLE;

		VkResult result = vkCreateSwapchainKHR(_device, &createInfo, nullptr, &_swapChain);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to create swap chain!");
		}

		vkGetSwapchainImagesKHR(_device, _swapChain, &imageCount, nullptr);
		_swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(_device, _swapChain, &imageCount, _swapChainImages.data());
		_swapChainImageFormat = surfaceFormat.format;
		_swapChainExtent = extent;
	}

	void createSurface() {
		VkResult result = glfwCreateWindowSurface(_instance, _window, nullptr, &_surface);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}
	}

	void createLogicalDevice() {
		QueueFamilyIndices indices = findQueueFamilies(_physicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		// 嬉しい: setなので、ファミリの重複は1つにまとめられる
		std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();

		createInfo.pEnabledFeatures = &deviceFeatures;

		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();

		// NOTE: 現在はVkInstanceとVkDeviceのvalidation layerが区別されていない
		//       よってenabledLayerCountとppEnabledLayerNamesは無視される
		//       互換性維持の為、一応設定
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			createInfo.enabledLayerCount = 0;
		}

		// 論理デバイスの作成
		VkResult result = vkCreateDevice(_physicalDevice, &createInfo, nullptr, &_device);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to create logical device");
		}

		// キューへのハンドルを取得
		vkGetDeviceQueue(_device, indices.graphicsFamily.value(), 0, &_graphicsQueue);
		vkGetDeviceQueue(_device, indices.presentFamily.value(), 0, &_presentQueue);
	}

	void pickPhysicalDevice() {
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
		if (deviceCount == 0) {
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());

		// デバイスの中から適合するデバイスを選び、使用することができる
		for (const auto& device : devices) {
			if (isDeviceSuitable(device)) {
				_physicalDevice = device;
				break;
			}
		}
		// 適合するものがなかったらエラー
		if (_physicalDevice == VK_NULL_HANDLE) {
			throw std::runtime_error("failed to find a suitable GPU!");
		}

		// デバイスのスコアランキングを算出し、1位を使用することができる
		// NOTE: スコアが小さい順に並ぶ
		std::multimap<int, VkPhysicalDevice> candidates;

		for (const auto& device : devices) {
			int score = rateDeviceSuitability(device);
			candidates.insert(std::make_pair(score, device));
		}

		// スコアランキング1位を使用
		if (candidates.rbegin()->first > 0) {
			_physicalDevice = candidates.rbegin()->second;
		}
		else {
			throw std::runtime_error("failed to find a suitable GPU!");
		}
	}

	// デバイスの性能を評価する
	// NOTE: 以下は実装の一例であり、様々な評価方法で実装できる
	int rateDeviceSuitability(VkPhysicalDevice device) {
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		int score = 0;
		// dGPUは、内蔵GPUより優れる
		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			score += 1000;
		}

		// Maximum possible size of textures affects graphics quality
		score += deviceProperties.limits.maxImageDimension2D;

		// Application can't function without geometry shaders
		if (!deviceFeatures.geometryShader) {
			return 0;
		}

		return score;
	}

	// swap chainの最適なformatを選択する
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
		for (const auto& availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	// swap chainの最適なpresentation modeを選択する
	/* ======== presentation mode 一覧 ==================
	VK_PRESENT_MODE_IMMEDIATE_KHR    : appが送信した画像を直ちに表示する。ティアリングを引き起こしうる。
	VK_PRESENT_MODE_FIFO_KHR		 : キュー構造。renderingした画像をenqueueし、ディスプレイがdequeueして表示する。キューが空の時、待機する。
	VK_PRESENT_MODE_FIFO_RELAXED_KHR : キューが空の時、appが送信した画像を直ちに表示する以外は1つ前と同じ。ティアリングを引き起こしうる。
	VK_PRESENT_MODE_MAILBOX_KHR      : キューが一杯の時、ブロッキングせずにキュー内の画像を新しいものに置き換える。所謂トリプルバッファリング。
	=================================================== */
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
		for (const auto& availablePresentMode : availablePresentModes) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return availablePresentMode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	// swap chainの解像度選択
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)()) {
			return capabilities.currentExtent;
		}
		else {
			int width, height;
			glfwGetFramebufferSize(_window, &width, &height);

			VkExtent2D actualExtent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	// デバイスが適合しているか？
	bool isDeviceSuitable(VkPhysicalDevice device) {
		/* 例: ジオメトリシェーダーが載ったdGPUのみサポートする場合
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
		
		return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			deviceFeatures.geometryShader;
		*/

		QueueFamilyIndices indices = findQueueFamilies(device);

		bool extensionsSupported = checkDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		// 拡張機能が利用可能であることを確認した後、swap chainの照会をする
		if (extensionsSupported) {
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() &&
								!swapChainSupport.presentModes.empty();
		}

		return indices.isComplete() && extensionsSupported && swapChainAdequate;
	}

	// アプリに必要な拡張機能(deviceExtensions, const std::vectorとしてソース冒頭で宣言)をdeviceがサポートしているか？
	bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	// すべてのキューファミリを検索する
	// 例: 計算コマンドの処理のみ許可するキューファミリ、
	//     メモリ転送関連のコマンドのみ許可するキューファミリ、等
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		uint32_t i = 0;
		for (const auto& queueFamily : queueFamilies) {
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphicsFamily = i;
			}

			// window surfaceに表示する機能を持つキューファミリを検索
			// NOTE: graphics Familyと同じqueue familyになる可能性が高いが、あえて別として扱う
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _surface, &presentSupport);
			if (presentSupport) {
				indices.presentFamily = i;
			}

			if (indices.isComplete()) {
				break;
			}

			++i;
		}

		return indices;
	}

	// swap chainのサポート内容を照会(query)する
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
		SwapChainSupportDetails details;
		
		// surface機能(画像の最小・最大数、等)の照会
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _surface, &details.capabilities);

		// format(色空間等)の照会
		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, nullptr);
		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, details.formats.data());
		}

		// presentation modeの照会
		uint32_t presentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, nullptr);
		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
	}

	void setupDebugMessenger() {
		if (!enableValidationLayers) return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo(createInfo);

		if (CreateDebugUtilsMessengerEXT(_instance, &createInfo, nullptr, &_debugMessenger) != VK_SUCCESS) {
			throw std::runtime_error("failed to set up debug messenger!");
		}
	}

	void mainLoop() {
		// ウィンドウを閉じざるを得ないエラーが発生するまで、ループする
		while (!glfwWindowShouldClose(_window))
		{
			glfwPollEvents();
		}
	}

	void cleanup() {
		for (auto framebuffer : _swapChainFramebuffers) {
			vkDestroyFramebuffer(_device, framebuffer, nullptr);
		}

		vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
		vkDestroyRenderPass(_device, _renderPass, nullptr);

		for (auto imageView : _swapChainImageViews) {
			vkDestroyImageView(_device, imageView, nullptr);
		}

		vkDestroySwapchainKHR(_device, _swapChain, nullptr);

		vkDestroyDevice(_device, nullptr);

		if (enableValidationLayers) {
			DestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
		}

		// NOTE: surface => instanceの順で破棄すること
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkDestroyInstance(_instance, nullptr);

		glfwDestroyWindow(_window);

		glfwTerminate();
	}

	void createInstance() {
		if (enableValidationLayers && !checkValidationLayerSupport()) {
			throw std::runtime_error("validation layers requested, but not available!");
		}

		VkApplicationInfo appInfo{};
		appInfo.sType				= VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName    = "Hello Triangle";
		appInfo.applicationVersion  = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName			= "No Engine";
		appInfo.engineVersion		= VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion			= VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType			= VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		// 有効な拡張機能の確認
		auto extensions = getRequiredExtension();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();
		
		// デバッグメッセンジャーの設定
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			populateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		} else {
			createInfo.enabledLayerCount = 0;

			createInfo.pNext = nullptr;
		}

		VkResult result = vkCreateInstance(
			&createInfo, /* 作成情報を持つ構造体へのポインタ */
			nullptr,	 /* custom allocator callbackへのポインタ */
			&_instance   /* オブジェクトへのハンドルを格納する変数へのポインタ */
		);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}
	}

	std::vector<VkExtensionProperties> getSupportedExtensions() {
		// 拡張機能の数だけを調べる
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		// 拡張機能を調べる
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		return extensions;
	}

	void showSupportedExtensions() {
		auto extensions = getSupportedExtensions();

		std::cout << "available extensions:" << std::endl;

		for (const auto& extension : extensions) {
			std::cout << "\t" << extension.extensionName << std::endl;
		}
	}

	bool containsSupportedExtension(
		const char** extensions,
		uint32_t extensionCount,
		const std::vector<VkExtensionProperties>& supported
	) {
		for (const auto& supportedExtension : supported) {
			for (uint32_t i = 0; i < extensionCount; ++i) {
				std::string strSupported = std::string(supportedExtension.extensionName);
				std::string strExtension = std::string(extensions[i]);
				if (strExtension == strSupported) {
					return true;
				}
			}
		}

		return false;
	}

	// すべてのvalidation layerがavailableか？
	bool checkValidationLayerSupport() {
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}

	// 検証レイヤーが有効かどうかに基づいて必要な拡張機能のリストを返す
	std::vector<const char*> getRequiredExtension() {
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData) {

		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}

	static std::vector<char> readFile(const std::string& filename) {
		std::ifstream file(
			filename,
			std::ios::ate | /* 末尾から読む */
			std::ios::binary /* バイナリを読む */
		);

		if (!file.is_open()) {
			throw std::runtime_error("failed to open file!");
		}

		// 末尾から読むので、現在位置がファイルサイズと等しい
		std::size_t fileSize = static_cast<std::size_t>(file.tellg());
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;
	}
};

int main() {
	HelloTriangleApplication app;

	try {
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
