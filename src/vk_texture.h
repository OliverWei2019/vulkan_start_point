#pragma once
#ifndef VK_TEXTURE_H
#define VK_TEXTURE_H
#include <vk_types.h>
#include <vk_engine.h>

namespace vkutil {
	void adjustImageLayout(VkCommandBuffer command, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t levelCount);

	void copyBufferToImage(VulkanEngine* engine,VkCommandBuffer cmd, VkBuffer stagineBuffer, VkImage image, VkExtent3D extent);

	VkCommandBuffer beginSigleTimeCommands(VulkanEngine* engine);

	void endSigleTimeCommands(VulkanEngine* engine, VkCommandBuffer commandBuffer);

	void generateMipmaps(VulkanEngine* engine, VkImage image, VkImageCreateInfo imageInfo);

	bool load_image_from_file(VulkanEngine* engine, const char* file, AllocatedImage& outImage);

	bool load_image_from_file(VulkanEngine* engine, const char* file, AllocatedImage& outImage, uint32_t& mipLevels);
}
#endif // !VK_TEXTURE_H
