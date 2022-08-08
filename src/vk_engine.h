// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once
#include <vector>
#include <string>
#include <deque>
#include <functional>
#include <chrono>

#include <vk_types.h>

#include <vk_pipelines.h>
#include <vk_mesh.h>
#include <vk_renderObjects.h>
#include <vk_frameData.h>

#include <imgui.h>
#include "imgui_impl_sdl.h"
#include "imgui_impl_vulkan.h"

#include "vk_descriptor.h"
//number of frames to overlap when rendering
constexpr unsigned int FRAME_OVERLAP = 2;

const std::string SHADER_SOURCE_PATH = "D:/VulKan/Vulkan_Engine/vulkan-guide-all-chapters/shaders/";
//const std::string SHADER_SOURCE_PATH = "D:/VulKan/Vulkanstart/shaders/";
const std::string ASSERT_SOURCE_PATH = "D:/VulKan/Vulkan_Engine/vulkan-guide-all-chapters/assets/";

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); //call the function
		}

		deletors.clear();
	}
};

class VulkanEngine {
public:

	bool _isInitialized{ false };
	int _frameNumber {0};
	int _selectedShader{ 0 };//select shader 
	//deletion queue include destroy function
	DeletionQueue _mainDeletionQueue;
	//Vulkan Memory Allocator Library(AMD) vma allocator
	VmaAllocator _allocator; 
	VkExtent2D _windowExtent{ 1200 , 800 };

	/*
	This is called a forward-declaration, 
	and it’s what allows us to have the SDL_Window pointer in the class, 
	without including SDL on the Vulkan engine header. 
	This variable holds the window that we create for the application.
	*/
	struct SDL_Window* _window{ nullptr };

	VkInstance _instance; // Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger; // Vulkan debug output handle
	VkPhysicalDevice _chosenGPU; // GPU chosen as the default device
	VkPhysicalDeviceProperties _gpuProperties; //gpu properties include data aligement
	VkDevice _device; // Vulkan device for commands
	VkSurfaceKHR _surface; // Vulkan window surface

	VkSwapchainKHR _swapchain; // from other articles

	// image format expected by the windowing system
	VkFormat _swapchainImageFormat;

	//array of images from the swapchain
	std::vector<VkImage> _swapchainImages;

	//array of image-views from the swapchain
	std::vector<VkImageView> _swapchainImageViews;

	VkQueue _graphicsQueue; //queue we will submit to
	uint32_t _graphicsQueueFamily; //family of that queue

	VkRenderPass _renderPass;//renderpass

	//frame data include command pool,command buffrs ,semaphore and fence
	//FrameData _frameData;
	//double frame storage
	FrameData _frames[FRAME_OVERLAP];

	std::vector<VkFramebuffer> _framebuffers; //framebuffers

	VkPipelineLayout _meshPipelineLayout;
	VkPipeline _meshPipeline;
	
	VkImageView _depthImageView;
	AllocatedImage _depthImage;

	//the format for the depth image
	VkFormat _depthFormat;
	//render pass create info connect attachments description
	std::vector< VkAttachmentDescription> _attachments;
	//renderpass subpass description vector multipass
	std::vector<VkSubpassDescription> _subpassDescription;
	//subpass dependecy 
	std::vector< VkSubpassDependency>_subpassDependency;

	//vkutil::DescriptorAllocator _textureDesciptorAllocator;
	//vkutil::DescriptorLayoutCache _descriptorLayoutCache;
	//vkutil::DescriptorBuilder _descriptorBuilder;

	//render objects set
	RenderObjectsSets _objectsSet;
	// global descriptor
	VkDescriptorSetLayout _globalSetLayout;
	VkDescriptorPool _descriptorPool;

	//scene object include paramets and buffer
	SceneObject _sceneObject;
	//object buffer (storage buffer)descriptor 
	VkDescriptorSetLayout _objectSetLayout;
	//upload vertex data into GPU
	UploadContext _uploadContext;

	//mipmap levels
	uint32_t mipLevels;
	//textures set
	std::unordered_map<std::string, Texture> _loadedTextures;
	// texture desriptorSet layout
	VkDescriptorSetLayout _singleTextureSetLayout;
	
public:

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

	//create buffer
	//AllocatedBuffer create_buffer(bool immediate_destroy, size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

/********/
	//load core vulkan structure;
	void init_vulkan();
	//initailize imgui
	void init_imgui();
	//initailze swap chain;
	void init_swapchain();
	//create command pool and command buffers
	void init_commands();

	void init_default_renderpass();

	void init_framebuffers();

	void init_sync_structures();

	//record command buffers
	void record_cmdbuffers(VkCommandBuffer& cmd, uint32_t imageIndex);

	//loads a shader module from a spir-v file. Returns false if it errors
	bool load_shader_module(const char* filePath, VkShaderModule* outShaderModule);
	
	//create uniform buffer and global descriptor based on frame numbers
	void init_descriptors();

	void init_pipelines();

	//
	void load_meshes();

	void upload_mesh(Mesh& mesh);

	//
	void init_scene();

	//draw render objects function
	void draw_objects(VkCommandBuffer cmd, RenderObject* first, int count);
	//getter for the frame we are rendering to right now.
	FrameData& get_current_frame();

	AllocatedBuffer create_buffer(bool immediate_destroy,size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	//tool function get the alignment boundary,
	size_t pad_uniform_buffer_size(size_t originalSize);

	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	void load_texture();

	void load_mipmap_texture();
	
};
