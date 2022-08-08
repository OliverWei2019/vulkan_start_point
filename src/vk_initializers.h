// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

namespace vkinit {

	//vulkan init code goes here
	VkCommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);

	VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shaderModule);

	VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info();

	VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info(VkPrimitiveTopology topology);

	VkPipelineRasterizationStateCreateInfo rasterization_state_create_info(VkPolygonMode polygonMode);

	VkPipelineMultisampleStateCreateInfo multisampling_state_create_info(VkSampleCountFlagBits sampleCount);

	VkPipelineColorBlendAttachmentState color_blend_attachment_state();

	VkPipelineLayoutCreateInfo pipeline_layout_create_info();

	VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags = 0);

	VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags = 0);

	VkShaderModuleCreateInfo shaderModule_create_info(size_t codeSize, const uint32_t* pCode);

	VmaAllocatorCreateInfo  vmaAllocator_create_info(VkPhysicalDevice& chosedGPU, VkDevice& device, VkInstance& instance);
	
	VkImageCreateInfo image_create_info(VkExtent3D extent, uint32_t miplevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage);

	VkImageViewCreateInfo imageview_create_info(VkImage image, VkFormat format, uint32_t miplevels,VkImageAspectFlags aspectFlags);

	VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info(bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp);
	
	VkDescriptorPoolCreateInfo descriptor_pool_create_info(const VkDescriptorPoolSize* poolSize, uint32_t count);

	VkDescriptorSetAllocateInfo descriptorSet_allocate_info(VkDescriptorPool& pool, const VkDescriptorSetLayout* setsLayouts);

	VkDescriptorSetLayoutBinding descriptorset_layout_binding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);

	VkWriteDescriptorSet write_descriptor_buffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, uint32_t binding);

	VkDescriptorSetLayoutCreateInfo descriptorSetLayout_create_info(uint32_t bindingCout, const VkDescriptorSetLayoutBinding* descriptorSetLayoutBinding);

	VkDescriptorBufferInfo descriptor_buffer_info(VkBuffer& buffer, uint32_t offset, uint32_t range);
	
	VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags = 0);
	
	VkSubmitInfo submit_info(VkCommandBuffer* cmd);

	VkSamplerCreateInfo sampler_create_info(VkFilter filters, VkSamplerAddressMode samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);
	
	VkWriteDescriptorSet write_descriptor_image(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding);

}

