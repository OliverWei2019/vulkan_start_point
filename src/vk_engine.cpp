#include "vk_engine.h"
#include <iostream>
#include <fstream>
#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
/// <summary>
/// include vulkan bootstrap library
/// </summary>
#include <VkBootstrap.h>
#include <vk_texture.h>
//we want to immediately abort when there is an error. 
//In normal engines this would give an error message to the user, 
//or perform a dump of state.
using namespace std;
#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			std::cout <<"Detected Vulkan error: " << err << std::endl; \
			abort();                                                \
		}                                                           \
	} while (0)

static glm::mat4 pushFunction(int frameNumber) {
	// make a model view matrix for rendering the object
	//camera position
	glm::vec3 camPos = { 0.f,0.f,-2.f };

	glm::mat4 view = glm::translate(glm::mat4(1.f), camPos);
	//camera projection
	glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f);
	projection[1][1] *= -1;
	//model rotation
	glm::mat4 model = glm::rotate(glm::mat4{ 1.0f }, glm::radians(frameNumber * 0.4f), glm::vec3(0, 1, 0));

	//calculate final mesh matrix
	glm::mat4 mesh_matrix = projection * view * model;
	return mesh_matrix;
}

void VulkanEngine::init()
{
	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);
	
	_window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_windowExtent.width,
		_windowExtent.height,
		window_flags
	);
	//load core vulkan structure;
	init_vulkan();

	//create swap chain;
	init_swapchain();
	//create command pool and command buffers
	init_commands();

	init_default_renderpass();

	init_framebuffers();

	init_sync_structures();
	
	init_descriptors();

	init_pipelines();

	//load texture
	load_mipmap_texture();
	//load mesh 
	load_meshes();

	init_scene();

	//initailize imgui
	init_imgui();
	//everything went fine
	_isInitialized = true;
}

void VulkanEngine::cleanup()
{	
	//if Window(SDL) is initialized,engine must destroy window
	if (_isInitialized) {
		// make sure the gpu has stopped doing its things
		vkDeviceWaitIdle(_device);
		//flush deletion queue and use vkDestroy**
		_mainDeletionQueue.flush();
		//Must destroy vma allocator in front of destroy physical device,device and vulkan instance
		//vma allocator created by chosed GPU,device and vulkan instance
		//vmaDestroyAllocator(_allocator);

		vkDestroyDevice(_device, nullptr);
		/*
		for (size_t i = 0; i < FRAME_OVERLAP; i++) {
			_frames[i]._globalDescrptorAllocator.cleanup();
			_frames[i]._objectDescrptorAllocator.cleanup();
		}
		_textureDesciptorAllocator.cleanup();
		_descriptorLayoutCache.cleanup();*/


		/*
		* VkPhysicalDevice can’t be destroyed, as it’s not a Vulkan resource per-se, 
		*it’s more like just a handle to a GPU in the system.
		*/
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		//vmaDestroyAllocator(_allocator);
		
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);

		SDL_DestroyWindow(_window);
	}
}
/*
* TODO: render code 
*/
void VulkanEngine::draw()
{	
	//call imgui::render()
	ImGui::Render();
	//wait until the GPU has finished rendering the last frame. Timeout of 1 second
	VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1000000000));
	VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

	//request image from the swapchain, one second timeout
	uint32_t swapchainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(_device,
		_swapchain, 
		1000000000, 
		get_current_frame()._presentSemaphore, 
		nullptr, 
		&swapchainImageIndex));
	//now that we are sure that the commands finished executing,
	record_cmdbuffers(get_current_frame()._mainCommandBuffer, swapchainImageIndex);

	//prepare the submission to the queue.
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext = nullptr;

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	submit.pWaitDstStageMask = &waitStage;

	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &get_current_frame()._presentSemaphore;

	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &get_current_frame()._renderSemaphore;

	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &get_current_frame()._mainCommandBuffer;

	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));
	// this will put the image we just rendered into the visible window.
	// we want to wait on the _renderSemaphore for that,
	// as it's necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;

	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = & get_current_frame()._renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

	//increase the number of frames drawn
	_frameNumber++;
}


void VulkanEngine::run()
{
	SDL_Event e;
	bool bQuit = false;

	//main loop
	//only stopped when SDL receives the SDL_QUIT event
	while (!bQuit)
	{
		//Handle events on queue
		//In here, we can check for things like keyboard events, 
		//mouse movement, window moving, minimization, and many others
		while (SDL_PollEvent(&e) != 0)
		{
			//close the window when user alt-f4s or clicks the X button			
			if (e.type == SDL_QUIT)
			{
				bQuit = true;
			}
			ImGui_ImplSDL2_ProcessEvent(&e);

			//other event handling
		}
		//imgui new frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame(_window);

		ImGui::NewFrame();


		//imgui commands
		ImGui::ShowDemoWindow();

		//your draw function
		draw();
	}
}

void VulkanEngine::init_vulkan() {
	//using vkbootsrap library in vkb namespace
	vkb::InstanceBuilder builder;

	//make the Vulkan instance, with basic debug features
	auto inst_ret = builder.set_app_name("Vulkan Engine Demo Example")
		.request_validation_layers(true) //using validation layer
		.require_api_version(1, 2, 0) //vulkan api versiong set 1.3.000
		.use_default_debug_messenger()
		.build();

	vkb::Instance vkb_inst = inst_ret.value();

	//store the instance(in vulkan engine class instance)
	_instance = vkb_inst.instance;
	//store the debug messenger
	//store the VkDebugUtilsMessengerEXT so can destroy it at program exit, 
	//otherwise it would leak it.
	_debug_messenger = vkb_inst.debug_messenger;

	// get the surface of the window we opened with SDL
	//_window created bu VulkanEngine:init_Window::SDL_CreateWindow() funtion
	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

	//use vkbootstrap to select a GPU using arg vkb::Instance(vkb_inst).
	//We want a GPU that can write to the SDL surface and supports Vulkan 1.1(spectify minimun version)
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	//select vulkan physical device (GPU)
	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 1)
		.set_surface(_surface)
		.select()
		.value();

	//create the final Vulkan logical device based physical device
	vkb::DeviceBuilder deviceBuilder{ physicalDevice };
	VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_parameters_features = {};
	shader_draw_parameters_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
	shader_draw_parameters_features.pNext = nullptr;
	shader_draw_parameters_features.shaderDrawParameters = VK_TRUE;
	vkb::Device vkbDevice = deviceBuilder.add_pNext(&shader_draw_parameters_features).build().value();

	//vkb::Device vkbDevice = deviceBuilder.build().value();

	// Get the VkDevice handle used in the rest of a Vulkan application
	_device = vkbDevice.device;
	_chosenGPU = physicalDevice.physical_device;
	_gpuProperties = physicalDevice.properties;

	// use vkbootstrap to get a Graphics queue
	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	//initialize the memory allocator based on GPU(physical device),device and Vulkan instance
	VmaAllocatorCreateInfo allocatorInfo = vkinit::vmaAllocator_create_info(_chosenGPU, _device, _instance);
	VK_CHECK(vmaCreateAllocator(&allocatorInfo, &_allocator));
	_mainDeletionQueue.push_function([=]() {
		vmaDestroyAllocator(_allocator);
		std::cout << "_allocator" << std::endl;
		});
}

void VulkanEngine::init_imgui()
{
	//1: create descriptor pool for IMGUI
	// the size of the pool is very oversize, but it's copied from imgui demo itself.
	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));


	// 2: initialize imgui library

	//this initializes the core structures of imgui
	ImGui::CreateContext();

	//this initializes imgui for SDL
	ImGui_ImplSDL2_InitForVulkan(_window);

	//this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = _instance;
	init_info.PhysicalDevice = _chosenGPU;
	init_info.Device = _device;
	init_info.Queue = _graphicsQueue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info, _renderPass);

	//execute a gpu command to upload imgui font textures
	immediate_submit([&](VkCommandBuffer cmd) {
		ImGui_ImplVulkan_CreateFontsTexture(cmd);
		});

	//clear font textures from cpu data
	ImGui_ImplVulkan_DestroyFontUploadObjects();

	//add the destroy the imgui created structures
	_mainDeletionQueue.push_function([=]() {
		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
		});
}

void VulkanEngine::init_swapchain() {
	//vkb build swapchain based on physical device,device and surface
	vkb::SwapchainBuilder swapchainBuilder{ _chosenGPU,_device,_surface };

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.use_default_format_selection()
		//use mail box present mode
		//This one has a list of images, 
		//and while one of them is being displayed by the screen, 
		//you will be continuously rendering to the others in the list. 
		//Whenever it’s time to display an image, 
		//it will select the most recent one. 
		//This is the one you use if you want Triple-buffering without hard vsync.
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		//send window size to swapchain,its also will create swapchain image and image size is locked!
		.set_desired_extent(_windowExtent.width, _windowExtent.height)
		.build()
		.value();

	//store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();

	_swapchainImageFormat = vkbSwapchain.image_format;

	//create _swapchain and then must push destroy swapchain funtion into deletion queue
	_mainDeletionQueue.push_function([=]() {
		vkDestroySwapchainKHR(_device, _swapchain, nullptr);
		std::cout << "_swapchain" << std::endl;
		});

	//depth image size will match the window
	VkExtent3D depthImageExtent = {
		_windowExtent.width,
		_windowExtent.height,
		1
	};

	//hardcoding the depth format to 32 bit float
	_depthFormat = VK_FORMAT_D32_SFLOAT;

	//the depth image will be an image with the format we selected and Depth Attachment usage flag
	VkImageCreateInfo dimg_info = vkinit::image_create_info(depthImageExtent,
		1,
		_depthFormat, 
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

	//for the depth image, we want to allocate it from GPU local memory
	VmaAllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	dimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//allocate and create the image
	VK_CHECK(vmaCreateImage(_allocator, &dimg_info, &dimg_allocinfo, &_depthImage._image, &_depthImage._allocation, nullptr));
	//add to deletion queues
	_mainDeletionQueue.push_function([=]() {
		vmaDestroyImage(_allocator, _depthImage._image, _depthImage._allocation);
		std::cout << "_depth image" << std::endl;
		});
	//build an image-view for the depth image to use for rendering
	VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthImage._image,
		_depthFormat,
		1,
		VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImageView));

	//add to deletion queues
	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, _depthImageView, nullptr);
		std::cout << "_depth image view" << std::endl;
		});

}

void VulkanEngine::init_commands() {
	//create upload context command pool
	VkCommandPoolCreateInfo uploadContextPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily);
	VK_CHECK(vkCreateCommandPool(_device, &uploadContextPoolInfo, nullptr, &_uploadContext._commandPool));
	_mainDeletionQueue.push_function([=]() {
		vkDestroyCommandPool(_device, _uploadContext._commandPool, nullptr);
		std::cout << "_upload context command pool" << std::endl;
		});
	//allocate the default command buffer that we will use for the instant commands
	VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_uploadContext._commandPool, 1);
	//VkCommandBuffer cmd;
	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_uploadContext._commandBuffer));
	// create draw object command pool
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, 
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	for (int i = 0; i < FRAME_OVERLAP; i++) {


		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

		//allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

		_mainDeletionQueue.push_function([=]() {
			vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
			std::cout << "frame["<<i<<"] command pool" << std::endl;
			});
	}
}

void VulkanEngine::init_default_renderpass() {
	// the renderpass will use this color attachment.
	VkAttachmentDescription color_attachment = {};
	//the attachment will have the format needed by the swapchain
	color_attachment.format = _swapchainImageFormat;
	//1 sample, we won't be doing MSAA
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	// we Clear when this attachment is loaded
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// we keep the attachment stored when the renderpass ends
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	//we don't care about stencil
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	//we don't know or care about the starting layout of the attachment
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	//after the renderpass ends, the image has to be on a layout ready for display
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	//push color attachment into dynamic array
	//render pass create infor using 
	_attachments.push_back(color_attachment);

	VkAttachmentReference color_attachment_ref = {};
	//attachment number will index into the pAttachments array in the parent renderpass itself
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depth_attachment = {};
	// Depth attachment
	depth_attachment.flags = 0;
	depth_attachment.format = _depthFormat;
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	//push depth attachment into dynamic array
	//render pass create infor using 
	_attachments.push_back(depth_attachment);

	VkAttachmentReference depth_attachment_ref = {};
	depth_attachment_ref.attachment = 1;
	depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	//we are going to create 1 subpass, which is the minimum you can do
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	subpass.pDepthStencilAttachment = &depth_attachment_ref;
	//push subpass 0 into _subpass dynamic array
	_subpassDescription.push_back(subpass);

	///image life
	/// UNDEFINED -> RenderPass Begins 
	/// -> Subpass 0 begins (Transition to Attachment Optimal) 
	/// -> Subpass 0 renders 
	/// -> Subpass 0 ends 
	/// -> Renderpass Ends (Transitions to Present Source)

	//add a subpass dependency that accesses to color attachments.
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	
	/*dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | 
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | 
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | 
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;*/
	//push dependency into subpass dependency dynamic array
	_subpassDependency.push_back(dependency);

	//add a new subpass dependency that "synchronizes"accesses to depth attachments.
	VkSubpassDependency depth_dependency = {};
	depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	depth_dependency.dstSubpass = 0;
	depth_dependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	depth_dependency.srcAccessMask = 0;
	depth_dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	depth_dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	//push depth dependency into subpass dependency dynamic array
	_subpassDependency.push_back(depth_dependency);

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

	//connect the color,depth attachment to the info
	render_pass_info.attachmentCount = _attachments.size();
	render_pass_info.pAttachments = _attachments.data();
	
	//connect the subpass to the info
	render_pass_info.subpassCount = _subpassDescription.size();
	render_pass_info.pSubpasses = _subpassDescription.data();

	//connect the subpass dependencies to the info
	render_pass_info.dependencyCount = _subpassDependency.size();
	render_pass_info.pDependencies = _subpassDependency.data();

	VK_CHECK(vkCreateRenderPass(_device, &render_pass_info, nullptr, &_renderPass));
	//After create object must push destroy function into deletion queue
	_mainDeletionQueue.push_function([=]() {
		vkDestroyRenderPass(_device, _renderPass, nullptr);
		std::cout << "render pass" << std::endl;
		});

}

void VulkanEngine::init_framebuffers() {
	//create the framebuffers for the swapchain images. 
	//This will connect the render-pass to the images for rendering
	VkFramebufferCreateInfo fb_info = {};
	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.pNext = nullptr;

	fb_info.renderPass = _renderPass;
	fb_info.attachmentCount = 1;
	fb_info.width = _windowExtent.width;
	fb_info.height = _windowExtent.height;
	fb_info.layers = 1;

	//grab how many images we have in the swapchain
	const uint32_t swapchain_imagecount = _swapchainImages.size();
	//_framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);
	_framebuffers.resize(swapchain_imagecount);

	//create framebuffers for each of the swapchain image views
	for (int i = 0; i < swapchain_imagecount; i++) {
		std::vector< VkImageView> attachments{
			_swapchainImageViews[i],
			_depthImageView
		};
		fb_info.attachmentCount = attachments.size();
		fb_info.pAttachments = attachments.data();
		VK_CHECK(vkCreateFramebuffer(_device, &fb_info, nullptr, &_framebuffers[i]));
		//After create object must push destroy function into deletion queue
		//swapchain image view also should destroy when destroy framebuffers
		_mainDeletionQueue.push_function([=]() {
			vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
			vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
			std::cout << "frame buffers" << std::endl;
			std::cout << "swapchain image views" << std::endl;
			});
	}
}

void VulkanEngine::init_sync_structures() {
	//create vertex mesh upload fence
	VkFenceCreateInfo uploadFenceCreateInfo = vkinit::fence_create_info();
	VK_CHECK(vkCreateFence(_device, &uploadFenceCreateInfo, nullptr, &_uploadContext._uploadFence));
	_mainDeletionQueue.push_function([=]() {
		vkDestroyFence(_device, _uploadContext._uploadFence, nullptr);
		std::cout << " _uploadContext._uploadFence" << std::endl;
		});
	//create synchronization structures
	//we want to create the fence with the Create Signaled flag, 
	//so we can wait on it before using it on a GPU command (for the first frame)
	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info(0);
	for (size_t i = 0; i < FRAME_OVERLAP; i++)
	{
		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));
		//After create object must push destroy function into deletion queue
		_mainDeletionQueue.push_function([=]() {
			vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
			std::cout << "_frames["<<i<<"]._renderFence" << std::endl;
			});
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._presentSemaphore));
		//After create object must push destroy function into deletion queue
		_mainDeletionQueue.push_function([=]() {
			vkDestroySemaphore(_device, _frames[i]._presentSemaphore, nullptr);
			std::cout << "_frames[" << i << "]._presentSemaphore" << std::endl;
			});
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));
		//After create object must push destroy function into deletion queue
		_mainDeletionQueue.push_function([=]() {
			vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
			std::cout << "_frames[" << i << "]._renderSemaphore" << std::endl;
			});
	}
}

void VulkanEngine::record_cmdbuffers(VkCommandBuffer& cmd, uint32_t imageIndex) {
	//now that we are sure that the commands finished executing,
	//we can safely reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(cmd, 0));
	//naming it cmd for shorter writing
	//VkCommandBuffer cmd = _mainCommandBuffer;

	//begin the command buffer recording.
	//We will use this command buffer exactly once, so we want to let Vulkan know that
	//so: flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	);
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	//make a clear-color from frame number. This will flash with a 120*pi frame period.
	VkClearValue clearValue;
	float flash = abs(sin(_frameNumber / 120.f));
	clearValue.color = { { 0.0f, 0.0f, flash, 1.0f } };
	
	//clear depth at 1
	VkClearValue depthClear;
	depthClear.depthStencil.depth = 1.f;
	std::vector<VkClearValue> _clearValue{
		clearValue,depthClear
	};

	//start the main renderpass.
	//We will use the clear color from above, and the framebuffer of the index the swapchain gave us
	VkRenderPassBeginInfo rpInfo = {};
	rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpInfo.pNext = nullptr;

	rpInfo.renderPass = _renderPass;
	rpInfo.renderArea.offset.x = 0;
	rpInfo.renderArea.offset.y = 0;
	rpInfo.renderArea.extent = _windowExtent;
	//index into the cached _framebuffers with the image index we got from the swapchain.
	rpInfo.framebuffer = _framebuffers[imageIndex];

	//connect clear values
	rpInfo.clearValueCount = _clearValue.size();
	rpInfo.pClearValues = _clearValue.data();

	vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

	////once we start adding rendering commands, they will go here
	//vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);
	//VkDeviceSize offset = 0;
	////vkCmdBindVertexBuffers(cmd, 0, 1, &_triangleMesh._vertexBuffer._buffer, &offset);
	//vkCmdBindVertexBuffers(cmd, 0, 1, &_monkeyMesh._vertexBuffer._buffer, &offset);
	////generate constants and push into vertex shader
	//MeshPushConstants constants;
	//constants.render_matrix = pushFunction(_frameNumber);
	//vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);
	//
	////vkCmdDraw(cmd, _triangleMesh._vertices.size(), 1,0, 0);
	//vkCmdDraw(cmd, _monkeyMesh._vertices.size(), 1, 0, 0);

	//draw object that int renderables order;
	draw_objects(cmd, _objectsSet._renderables.data(), _objectsSet._renderables.size());
	//call imgui draw function
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
	//finalize the render pass
	vkCmdEndRenderPass(cmd);
	//finalize the command buffer (we can no longer add commands,
	//but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

}

//loads a shader module from a spir-v file. Returns false if it errors
bool VulkanEngine::load_shader_module(const char* filePath, VkShaderModule* outShaderModule) {
	//open the file. With cursor at the end
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		std::cout << "Failed open shader file" << std::endl;
		return false;
	}

	//find what the size of the file is by looking up the location of the cursor
	//because the cursor is at the end, it gives the size directly in bytes
	size_t fileSize = (size_t)file.tellg();

	//spirv expects the buffer to be on uint32, so make sure to reserve a int vector big enough for the entire file
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	//put file cursor at beggining
	file.seekg(0);

	//load the entire file into the buffer
	file.read((char*)buffer.data(), fileSize);

	//now that the file is loaded into the buffer, we can close it
	file.close();

	//create a new shader module, using the buffer we loaded

	VkShaderModuleCreateInfo createInfo = vkinit::shaderModule_create_info(buffer.size() * sizeof(uint32_t),
		buffer.data());

	//check that the creation goes well.
	VkShaderModule shaderModule;
	if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		return false;
	}
	*outShaderModule = shaderModule;
	return true;
}

void VulkanEngine::init_descriptors() {
	//_descriptorLayoutCache.init(_device);

	//information about the binding.
	// set0 binding point 0
	// it's a uniform buffer binding
	// we use it from the vertex shader
	VkDescriptorSetLayoutBinding camBufferBinding = vkinit::descriptorset_layout_binding(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0
	);
	// set0 binding point 1
	// it's a dynamic uniform buffer binding
	// we use it from the vertex shader and fragament shader
	VkDescriptorSetLayoutBinding sceneBufferBinding = vkinit::descriptorset_layout_binding(
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
		1
	);
	VkDescriptorSet objectSet;
	// set1 binding point 0
	// it's a storage buffer binding
	// we use it from the vertex shader
	VkDescriptorSetLayoutBinding objectBufferBinding = vkinit::descriptorset_layout_binding(
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		VK_SHADER_STAGE_VERTEX_BIT,
		0
	);

	//set2 binding point 0
	//it's a texture view binding and sampler
	//using it at fragment shader

	VkDescriptorSetLayoutBinding textureBinding =
		vkinit::descriptorset_layout_binding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			0
		);

	std::vector<VkDescriptorSetLayoutBinding> globalLayoutBindSet{
		camBufferBinding,sceneBufferBinding
	};
	VkDescriptorSetLayoutCreateInfo globalSetinfo = vkinit::descriptorSetLayout_create_info(
		globalLayoutBindSet.size(), globalLayoutBindSet.data()
	);
	VK_CHECK(vkCreateDescriptorSetLayout(_device, &globalSetinfo, nullptr, &_globalSetLayout));
	// add descriptor set layout to deletion queues
	_mainDeletionQueue.push_function([&]() {
		vkDestroyDescriptorSetLayout(_device, _globalSetLayout, nullptr);
		std::cout << "_globalSetLayout" << std::endl;
		});
	
	//create objects descriptor set layout
	std::vector<VkDescriptorSetLayoutBinding> objectLayoutBindSet{
		objectBufferBinding
	};
	VkDescriptorSetLayoutCreateInfo objectSetInfo = vkinit::descriptorSetLayout_create_info(
		objectLayoutBindSet.size(), objectLayoutBindSet.data()
	);
	VK_CHECK(vkCreateDescriptorSetLayout(_device, &objectSetInfo, nullptr, &_objectSetLayout));
	_mainDeletionQueue.push_function([&]() {
		vkDestroyDescriptorSetLayout(_device, _objectSetLayout, nullptr);
		std::cout << "_objectSetLayout" << std::endl;
		});
	

	//create texture descriptor set layout
	std::vector<VkDescriptorSetLayoutBinding> textureLayoutBindSet{
		textureBinding
	};
	VkDescriptorSetLayoutCreateInfo textureSetInfo = vkinit::descriptorSetLayout_create_info(
		textureLayoutBindSet.size(), textureLayoutBindSet.data()
	);
	//_singleTextureSetLayout = _descriptorLayoutCache.create_descriptor_layout(&textureSetInfo);
	VK_CHECK(vkCreateDescriptorSetLayout(_device, &textureSetInfo, nullptr, &_singleTextureSetLayout));
	_mainDeletionQueue.push_function([&]() {
		vkDestroyDescriptorSetLayout(_device, _singleTextureSetLayout, nullptr);
		std::cout << "_singleTextureSetLayout" << std::endl;
		});
	
	//create a descriptor pool that will hold 10 uniform buffers
	std::vector<VkDescriptorPoolSize> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,10}
	};
	VkDescriptorPoolCreateInfo pool_info = vkinit::descriptor_pool_create_info(
		sizes.data(), (uint32_t)sizes.size()
	);
	vkCreateDescriptorPool(_device, &pool_info, nullptr, &_descriptorPool);
	// add descriptor set layout to deletion queues
	_mainDeletionQueue.push_function([&]() {
		vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
		std::cout << "_descriptorPool" << std::endl;
		});
	
	//create scene dynamic buffer
	//get buffer size based on frame numbers and aligement size
	const size_t sceneParamBufferSize = FRAME_OVERLAP * pad_uniform_buffer_size(sizeof(GPUSceneData));
	_sceneObject._sceneParameterBuffer = create_buffer(
		false,
		sceneParamBufferSize,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);
	for (size_t i = 0; i < FRAME_OVERLAP; i++) {
		//create storage object data buffer
		const size_t MAX_OBJECTS_NUM = 10000;
		_frames[i].objectBuffer = create_buffer(
			false,
			sizeof(GPUObjectData) * MAX_OBJECTS_NUM,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);
		//create camera buffer(uniform buffer)
		_frames[i].cameraBuffer = create_buffer(
			false,
			sizeof(GPUCameraData),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);
		//information about the buffer we want to point at in the descriptor
		//descriptor connect to source(uniform buffer)
		//descriptor buffer info is a point of buffer
		//descriptor include info of source(buffer,image,imageview)
		VkDescriptorBufferInfo cameraBuffer = vkinit::descriptor_buffer_info(
			_frames[i].cameraBuffer._buffer, 0, sizeof(GPUCameraData)
		);
		VkDescriptorBufferInfo sceneBuffer = vkinit::descriptor_buffer_info(
			_sceneObject._sceneParameterBuffer._buffer, 0, sizeof(GPUSceneData)
		);
		// storage buffer(object buffer) descriptor info
		VkDescriptorBufferInfo objectBuffer = vkinit::descriptor_buffer_info(
			_frames[i].objectBuffer._buffer, 0, sizeof(GPUObjectData) * MAX_OBJECTS_NUM
		);

		/*_frames[i]._globalDescrptorAllocator.init(_device);

		vkutil::DescriptorBuilder::begin(&_descriptorLayoutCache, &_frames[i]._globalDescrptorAllocator)
			.bind_buffer(0, &cameraBuffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
			.bind_buffer(1, &sceneBuffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(_frames[i].globalDescriptor);

		_frames[i]._objectDescrptorAllocator.init(_device);
		vkutil::DescriptorBuilder::begin(&_descriptorLayoutCache, &_frames[i]._objectDescrptorAllocator)
			.bind_buffer(0, &objectBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
			.build(_frames[i].objectDescriptor);*/

		//allocate one descriptor set for each frame base on global layout and descriptor pool
		//allocate one descriptor set for each frame
		VkDescriptorSetAllocateInfo globalAllocInfo = vkinit::descriptorSet_allocate_info(
			_descriptorPool, &_globalSetLayout
		);
		vkAllocateDescriptorSets(_device, &globalAllocInfo, &_frames[i].globalDescriptor);
		
		//allocate object buffers descriptor set 
		VkDescriptorSetAllocateInfo objectAllocInfo = vkinit::descriptorSet_allocate_info(
			_descriptorPool, &_objectSetLayout
		);
		vkAllocateDescriptorSets(_device, &objectAllocInfo, &_frames[i].objectDescriptor);
		
		VkWriteDescriptorSet cameraWrite = vkinit::write_descriptor_buffer(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			_frames[i].globalDescriptor,
			&cameraBuffer,
			0
		);
		VkWriteDescriptorSet sceneWrite = vkinit::write_descriptor_buffer(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			_frames[i].globalDescriptor,
			&sceneBuffer,
			1
		);
		VkWriteDescriptorSet objectWrite = vkinit::write_descriptor_buffer(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			_frames[i].objectDescriptor,
			&objectBuffer,
			0
		);
		std::vector<VkWriteDescriptorSet> setWrites{
			cameraWrite,sceneWrite,objectWrite
		};
		vkUpdateDescriptorSets(_device, setWrites.size(), setWrites.data(), 0, nullptr);
	}
}

void VulkanEngine::init_pipelines() {
	VkShaderModule triangleVertexShader;
	if (!load_shader_module((SHADER_SOURCE_PATH +"tri_mesh_ssbo.vert.spv").c_str(), &triangleVertexShader))
	{
		std::cout << "Error when building the vertex shader module" << std::endl;

	}
	else {
		std::cout << "mesh vertex shader successfully loaded" << std::endl;
	}
	VkShaderModule texturedMeshShader;
	if (!load_shader_module((SHADER_SOURCE_PATH + "textured_lit.frag.spv").c_str(), &texturedMeshShader))
	{
		std::cout << "Error when building the textured mesh shader" << std::endl;
	}
	else {
		std::cout << "textured fragemnet shader successfully loaded" << std::endl;
	}

	//build the pipeline layout that controls the inputs / outputs of the shader
	//we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
	
	//we start from just the default empty pipeline layout info
	VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();

	//setup push constants
	VkPushConstantRange push_constant;
	//this push constant range starts at the beginning
	push_constant.offset = 0;
	//this push constant range takes up the size of a MeshPushConstants struct
	push_constant.size = sizeof(MeshPushConstants);
	//this push constant range is accessible only in the vertex shader
	push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;
	mesh_pipeline_layout_info.pushConstantRangeCount = 1;

	//add descriptot set layout info
	// 1 _globalLayout 2 _objectLayout
	std::vector<VkDescriptorSetLayout> descpSetLayouts{
		_globalSetLayout,_objectSetLayout
	};
	mesh_pipeline_layout_info.setLayoutCount = descpSetLayouts.size();
	mesh_pipeline_layout_info.pSetLayouts = descpSetLayouts.data();

	VK_CHECK(vkCreatePipelineLayout(_device, &mesh_pipeline_layout_info, nullptr, &_meshPipelineLayout));
	//remember to destroy the pipeline layout
	_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
		});

	//build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
	PipelineBuilder pipelineBuilder;

	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, triangleVertexShader));

	//vertex input controls how to read vertices from vertex buffers. We aren't using it yet
	pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();

	//input assembly is the configuration for drawing triangle lists, strips, or individual points.
	//we are just going to draw triangle list
	pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	//build viewport and scissor from the swapchain extents
	pipelineBuilder._viewport.x = 0.0f;
	pipelineBuilder._viewport.y = 0.0f;
	pipelineBuilder._viewport.width = (float)_windowExtent.width;
	pipelineBuilder._viewport.height = (float)_windowExtent.height;
	pipelineBuilder._viewport.minDepth = 0.0f;
	pipelineBuilder._viewport.maxDepth = 1.0f;

	pipelineBuilder._scissor.offset = { 0, 0 };
	pipelineBuilder._scissor.extent = _windowExtent;

	//configure the rasterizer to draw filled triangles
	pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

	//we don't use multisampling, so just run the default one
	pipelineBuilder._multisampling = vkinit::multisampling_state_create_info(VK_SAMPLE_COUNT_1_BIT);

	//a single blend attachment with no blending and writing to RGBA
	pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();

	//default depthtesting
	pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
	//***build the mesh pipeline
	
	VertexInputDescription vertexDescrption = Vertex::get_vertex_description();
	pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = vertexDescrption.bindings.size();
	pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions = vertexDescrption.bindings.data();
	pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount = vertexDescrption.attributes.size();
	pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions = vertexDescrption.attributes.data();

	//create other textured pipeline
	//first create new texture pipeline layout 
	//std::vector<VkDescriptorSetLayout> descpSetLayouts = _descriptorLayoutCache.grab_Layout();
	descpSetLayouts.push_back(_singleTextureSetLayout);
	VkPipelineLayoutCreateInfo texture_pipeline_layout_info = mesh_pipeline_layout_info;
	texture_pipeline_layout_info.setLayoutCount = descpSetLayouts.size();
	texture_pipeline_layout_info.pSetLayouts = descpSetLayouts.data();
	VkPipelineLayout texturePipelineLayout;
	VK_CHECK(vkCreatePipelineLayout(_device, &texture_pipeline_layout_info, nullptr, &texturePipelineLayout));
	//remember to destroy the pipeline layout
	_mainDeletionQueue.push_function([=]() {
		vkDestroyPipelineLayout(_device, texturePipelineLayout, nullptr);
		});
	pipelineBuilder._pipelineLayout = texturePipelineLayout;
	pipelineBuilder._shaderStages.clear();
	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, triangleVertexShader)
	);
	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, texturedMeshShader)
	);
	VkPipeline texPipeline = pipelineBuilder.build_pipeline(_device, _renderPass);
	_mainDeletionQueue.push_function([=]() {
		vkDestroyPipeline(_device, texPipeline, nullptr);
		});
	_objectsSet.create_material(texPipeline,texturePipelineLayout, "texturedmesh");


	//destroy shadermodule
	//destroy all shader modules, outside of the queue
	vkDestroyShaderModule(_device, triangleVertexShader, nullptr);
	vkDestroyShaderModule(_device, texturedMeshShader, nullptr);

}

void VulkanEngine::load_meshes() {

	Mesh _lostEmpire;
	
	//_lostEmpire.load_from_obj((ASSERT_SOURCE_PATH + "lost_empire.obj").c_str());
	_lostEmpire.load_from_obj("D:/VulKan/Vulkanstart/models/viking_room.obj");
	//upload lost empire object
	upload_mesh(_lostEmpire);

	_objectsSet._meshes["empire"] = _lostEmpire;
}

void VulkanEngine::upload_mesh(Mesh& mesh) {
	//upload mesh vertex data to device(gpu) local memory
	const size_t bufferSize = mesh._vertices.size() * sizeof(Vertex);
	//create straging buffer storage vertex that load from disk
	//straging buffer need destroy immediately
	AllocatedBuffer stagingBuffer = create_buffer(
		true,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VMA_MEMORY_USAGE_CPU_ONLY);
	//copy vertex data to straging buffer
	void* data;
	vmaMapMemory(_allocator, stagingBuffer._allocation, &data);
	memcpy(data, mesh._vertices.data(), bufferSize);
	vmaUnmapMemory(_allocator, stagingBuffer._allocation);

	//create device local memory buffer(vertex buffer)
	mesh._vertexBuffer = create_buffer(
		false,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY
	);
	//submit copy command buffer->copy straging buffer data to gpu local buffer
	immediate_submit([=](VkCommandBuffer cmd) {
		VkBufferCopy bufferCopy;
		bufferCopy.srcOffset = 0;
		bufferCopy.dstOffset = 0;
		bufferCopy.size = bufferSize;
		vkCmdCopyBuffer(cmd, stagingBuffer._buffer, mesh._vertexBuffer._buffer, 1, &bufferCopy);
		});

	//remember:must destroy straging buffer
	vmaDestroyBuffer(_allocator, stagingBuffer._buffer, stagingBuffer._allocation);
}

void VulkanEngine::init_scene() {
	
	RenderObject map;
	map.mesh = _objectsSet.get_mesh("empire");
	map.material = _objectsSet.get_material("texturedmesh");
	map.transformMatrix = glm::translate(glm::vec3(1.0f));

	_objectsSet._renderables.push_back(map);
	
	//create a sampler for the texture
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);
	samplerInfo.maxAnisotropy = _gpuProperties.limits.maxSamplerAnisotropy;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;//使用归一化[0-1)坐标
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.maxLod = static_cast<float>(mipLevels);
	samplerInfo.minLod = 0.0f;
	VkSampler blockySampler;
	VK_CHECK(vkCreateSampler(_device, &samplerInfo, nullptr, &blockySampler));
	_mainDeletionQueue.push_function([=]() {
		vkDestroySampler(_device, blockySampler, nullptr);
		std::cout << "blockySampler" << std::endl;
		});
	Material* texturedMat = _objectsSet.get_material("texturedmesh");

	//allocate the descriptor set for single-texture to use on the material
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.pNext = nullptr;
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = _descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &_singleTextureSetLayout;

	vkAllocateDescriptorSets(_device, &allocInfo, &texturedMat->textureSet);

	//write to the descriptor set so that it points to our empire_diffuse texture
	VkDescriptorImageInfo imageBufferInfo;
	imageBufferInfo.sampler = blockySampler;
	imageBufferInfo.imageView = _loadedTextures["empire_diffuse"].imageView;
	imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	//_textureDesciptorAllocator.init(_device);
	////VkDescriptorSet textureSet;
	//vkutil::DescriptorBuilder::begin(&_descriptorLayoutCache, &_textureDesciptorAllocator)
	//	.bind_image(0, &imageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
	//	.build((*texturedMat).textureSet);

	VkWriteDescriptorSet texture1 = vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texturedMat->textureSet, &imageBufferInfo, 0);

	vkUpdateDescriptorSets(_device, 1, &texture1, 0, nullptr);
}

void VulkanEngine::draw_objects(VkCommandBuffer cmd, RenderObject* first, int count) {
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	//make a model view matrix for rendering the object
	//
	glm::mat4 model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	//camera view
	glm::vec3 camPos = { 0.f,-6.0f,-10.0f };
	//glm::mat4 view = glm::translate(glm::mat4(1.f), camPos);
	glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	//camera projection
	//glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f);
	glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)_windowExtent.width / (float)_windowExtent.height, 0.1f, 10.0f);
	projection[1][1] *= -1;
	//fill a GPU camera data struct
	GPUCameraData camData;
	camData.proj = projection;
	camData.view = view;
	camData.viewproj = projection * view;

	//and copy it to the buffer
	void* data;
	vmaMapMemory(_allocator, get_current_frame().cameraBuffer._allocation, &data);
	memcpy(data, &camData, sizeof(GPUCameraData));
	vmaUnmapMemory(_allocator, get_current_frame().cameraBuffer._allocation);
	float framed = (_frameNumber / 120.f);

	_sceneObject._sceneParameters.ambientColor = { sin(framed),0,cos(framed),1 };

	char* sceneData;
	vmaMapMemory(_allocator, _sceneObject._sceneParameterBuffer._allocation, (void**)&sceneData);

	int frameIndex = _frameNumber % FRAME_OVERLAP;

	sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;

	memcpy(sceneData, &_sceneObject._sceneParameters, sizeof(GPUSceneData));

	vmaUnmapMemory(_allocator, _sceneObject._sceneParameterBuffer._allocation);

	//copy object transformMatrix to object buffer model matrix
	void* objectData;
	vmaMapMemory(_allocator, get_current_frame().objectBuffer._allocation, &objectData);
	GPUObjectData* objectSSBO = (GPUObjectData*)objectData;
	for (int i = 0; i < count; i++) {
		RenderObject& object = first[i];
		//objectSSBO[i].modelMatrix = object.transformMatrix;
		objectSSBO[i].modelMatrix = model;
	}
	vmaUnmapMemory(_allocator, get_current_frame().objectBuffer._allocation);

	Mesh* lastMesh = nullptr;
	Material* lastMaterial = nullptr;
	for (int i = 0; i < count; i++)
	{
		//get a render object from objects set render table;
		RenderObject& object = first[i];
		//objectSSBO[i].modelMatrix = object.transformMatrix;

		//only bind the pipeline if it doesn't match with the already bound one
		//if material(pipeline and pipeline yaout) is same,must not bind pipeline again!
		if (object.material != lastMaterial) {

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
			lastMaterial = object.material;
			//offset for our scene buffer based on frame index
			uint32_t uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
			//bind the descriptor set when changing pipeline
			//bind the global descriptor set in pipeline layout 0 index
			vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
				object.material->pipelineLayout,0,1,
				&get_current_frame().globalDescriptor,1,&uniform_offset);
			//bind the object descriptor set in pipeline layout 1 index
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
				object.material->pipelineLayout, 1, 1,
				&get_current_frame().objectDescriptor, 0, nullptr);
			//bind the texture descriptor set in pipeline layout 2 index
			if (object.material->textureSet != VK_NULL_HANDLE) {
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
					object.material->pipelineLayout, 2, 1, &object.material->textureSet, 0, nullptr);
			};
			//_descriptorLayoutCache.
		}


		//glm::mat4 model = object.transformMatrix;
		//final render matrix, that we are calculating on the cpu
		glm::mat4 mesh_matrix = projection * view * model;

		MeshPushConstants constants ;
		constants.render_matrix = mesh_matrix;

		//upload the mesh to the GPU via push constants
		if (object.material)
		{
			vkCmdPushConstants(cmd, object.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);
		}
		//only bind the mesh if it's a different one from last bind
		if (object.mesh != lastMesh) {
			//bind the mesh vertex buffer with offset 0
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->_vertexBuffer._buffer, &offset);
			lastMesh = object.mesh;
		}
		//we can now draw
		if (object.mesh)
		{
			//NOTE: i mean vertex shader gl_instance input
			vkCmdDraw(cmd, object.mesh->_vertices.size(), 1, 0, i);
		}
	}
}

FrameData& VulkanEngine::get_current_frame() {
	return _frames[_frameNumber % FRAME_OVERLAP];
}

AllocatedBuffer VulkanEngine::create_buffer(bool immediate_destroy,size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
	//allocate vertex buffer
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;

	bufferInfo.size = allocSize;
	bufferInfo.usage = usage;


	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = memoryUsage;

	AllocatedBuffer newBuffer;

	//allocate the buffer
	VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo,
		&newBuffer._buffer,
		&newBuffer._allocation,
		nullptr));
	//whether destroy buffer or not immediately
	//immediately destroy -> not push desctroy function to main deletion queue
	//not immediately destory->push desctroy function to main deletion queue
	if (!immediate_destroy) {
		_mainDeletionQueue.push_function([=]() {
			vmaDestroyBuffer(_allocator, newBuffer._buffer, newBuffer._allocation);
			std::cout << "newBuffer" << std::endl;
			});
	}
	return newBuffer;
}

size_t VulkanEngine::pad_uniform_buffer_size(size_t originalSize)
{
	// Calculate required alignment based on minimum device offset alignment
	size_t minUboAlignment = _gpuProperties.limits.minUniformBufferOffsetAlignment;
	size_t alignedSize = originalSize;
	if (minUboAlignment > 0) {
		alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}
	return alignedSize;
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function) {
	VkCommandBuffer cmd = _uploadContext._commandBuffer;
	VkCommandBufferBeginInfo cmdBufferBeginInfo = vkinit::command_buffer_begin_info();
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBufferBeginInfo));
	//exceute function
	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	//submit recoreded cmd
	VkSubmitInfo submitInfo = vkinit::submit_info(&cmd);
	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _uploadContext._uploadFence));

	//waite fence and reset fence
	vkWaitForFences(_device, 1, &_uploadContext._uploadFence, true, 9999999999);
	vkResetFences(_device, 1, &_uploadContext._uploadFence);
	// reset the command buffers inside the command pool
	vkResetCommandPool(_device, _uploadContext._commandPool, 0);
}
// call load texture function before init_scene function
void VulkanEngine::load_texture() {
	Texture lostEmpire;
	//const char* file_name = (ASSERT_SOURCE_PATH + "lost_empire-RGBA.png").c_str();
	const char* file_name = "D:/VulKan/Vulkanstart/textures/viking_room.png";

	if (!vkutil::load_image_from_file((VulkanEngine*)(this), file_name, lostEmpire.image)) {
		std::cerr << "Failed to load image from file!" << std::endl;
		return;
	}
	//create image view because of cant access image directly
	VkImageViewCreateInfo imageinfo = vkinit::imageview_create_info(lostEmpire.image._image,VK_FORMAT_R8G8B8A8_SRGB, 1,VK_IMAGE_ASPECT_COLOR_BIT);
	vkCreateImageView(_device, &imageinfo, nullptr, &lostEmpire.imageView);
	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, lostEmpire.imageView, nullptr);
		std::cout << "lostEmpire.imageView" << std::endl;
		});
	//add texture to textures set
	_loadedTextures["empire_diffuse"] = lostEmpire;
}

void VulkanEngine::load_mipmap_texture() {
	Texture lostEmpire;
	//const char* file_name = (ASSERT_SOURCE_PATH + "lost_empire-RGBA.png").c_str();
	const char* file_name = "D:/VulKan/Vulkanstart/textures/viking_room.png";

	if (!vkutil::load_image_from_file((VulkanEngine*)(this),file_name, lostEmpire.image,mipLevels)) {
		std::cerr << "Failed to load image from file!" << std::endl;
		return;
	}
	//create image view because of cant access image directly
	VkImageViewCreateInfo imageinfo = vkinit::imageview_create_info(lostEmpire.image._image, VK_FORMAT_R8G8B8A8_SRGB, mipLevels, VK_IMAGE_ASPECT_COLOR_BIT);
	vkCreateImageView(_device, &imageinfo, nullptr, &lostEmpire.imageView);
	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, lostEmpire.imageView, nullptr);
		std::cout << "lostEmpire.imageView" << std::endl;
		});
	//add texture to textures set
	_loadedTextures["empire_diffuse"] = lostEmpire;
}


