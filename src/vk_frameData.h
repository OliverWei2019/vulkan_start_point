#pragma once
#ifndef VK_FRAMEDATA_H
#define VK_FRAMEDATA_H
#include <vk_types.h>
#include <glm/vec3.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <vk_descriptor.h>
struct GPUObjectData {
	glm::mat4 modelMatrix;
};
struct GPUCameraData {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewproj;
};
//
struct GPUSceneData {
	glm::vec4 fogColor; // w is for exponent
	glm::vec4 fogDistances; //x for min, y for max, zw unused.
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection; //w for sun power
	glm::vec4 sunlightColor;
};
//
struct SceneObject {
	GPUSceneData _sceneParameters;
	AllocatedBuffer _sceneParameterBuffer;
};
//
struct FrameData
{
	//semaphore 
	VkSemaphore _presentSemaphore, _renderSemaphore;
	//fence
	VkFence _renderFence;
	//command pool and command buffer;
	VkCommandPool _commandPool; //the command pool for our commands
	VkCommandBuffer _mainCommandBuffer; //the buffer we will record into
	//MVP matrix buffer that shader using
	//buffer that holds a single GPUCameraData to use when rendering
	
	AllocatedBuffer cameraBuffer;
	//every frame has a gloablDescriptor
	VkDescriptorSet globalDescriptor;
	//vkutil::DescriptorAllocator _globalDescrptorAllocator;

	//storage object data (module matrix)buffer
	AllocatedBuffer objectBuffer;
	VkDescriptorSet objectDescriptor;
	//vkutil::DescriptorAllocator _objectDescrptorAllocator;
};
#endif // !VK_FRAMEDATA_H
