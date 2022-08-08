#include "vk_texture.h"
#include <iostream>

#include <vk_initializers.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

void vkutil::adjustImageLayout(VkCommandBuffer command, VkImage image, VkImageLayout oldLayout,
    VkImageLayout newLayout, uint32_t levelCount)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = levelCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
        && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(
        command,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

void vkutil::copyBufferToImage(VulkanEngine* engine,VkCommandBuffer cmd, VkBuffer stagingBuffer, VkImage image, VkExtent3D extent) {
    //set copy buffer data to image command buffer
    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = extent;
    //copy the buffer into the image
    vkCmdCopyBufferToImage(
        cmd,
        stagingBuffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copyRegion);
    vkutil::endSigleTimeCommands(engine, cmd);
}
VkCommandBuffer vkutil::beginSigleTimeCommands(VulkanEngine* engine) {
    //录制commanfBuffer起始配置
    VkCommandBufferAllocateInfo allocInfo = vkinit::command_buffer_allocate_info(
        engine->_uploadContext._commandPool, 1, VK_COMMAND_BUFFER_LEVEL_PRIMARY
    );
    VkCommandBuffer cmd;
    if (vkAllocateCommandBuffers(engine->_device, &allocInfo, &cmd) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
    VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info(
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    );
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin record copy buffer command !");
    }
    return cmd;
}

void vkutil::endSigleTimeCommands(VulkanEngine* engine,VkCommandBuffer commandBuffer) {
    //录制commanfBuffer结束配置
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(engine->_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("failed to submite copybuffer command queue !");
    }
    vkQueueWaitIdle(engine->_graphicsQueue);
    //waite fence and reset fence
    //vkWaitForFences(engine->_device, 1, &engine->_uploadContext._uploadFence, true, 9999999999);
    //vkResetFences(engine->_device, 1, &engine->_uploadContext._uploadFence);
    //把copyBuffer command 还给 command pool
    vkFreeCommandBuffers(engine->_device,engine->_uploadContext._commandPool, 1, &commandBuffer);
}

void vkutil::generateMipmaps(VulkanEngine* engine,VkImage image, VkImageCreateInfo imageInfo) {
    // Check if image format supports linear blitting
    VkFormat imageFormat = imageInfo.format;
    VkFormatProperties formatProp{};
    vkGetPhysicalDeviceFormatProperties(engine->_chosenGPU, imageFormat, &formatProp);
    if (!(formatProp.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }
    //VkCommandBuffer commandBuffer = engine->_uploadContext._commandBuffer;
    VkCommandBuffer commandBuffer = vkutil::beginSigleTimeCommands(engine);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;
    VkExtent3D extent = imageInfo.extent;
    int32_t mipWidth = extent.width;
    int32_t mipHeight = extent.height;
    uint32_t miplevels = imageInfo.mipLevels;
    for (uint32_t i = 1; i < miplevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
        VkImageBlit blit{};
        blit.srcOffsets[0] = { 0,0,0 }; //起始偏移量和终点偏移量确定一个三维方格区域
        blit.srcOffsets[1] = { mipWidth,mipHeight,1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.srcSubresource.mipLevel = i - 1;

        blit.dstOffsets[0] = { 0,0,0 }; //起始偏移量和终点偏移量确定一个三维方格区域
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1,mipHeight > 1 ? mipHeight / 2 : 1,1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;
        blit.dstSubresource.mipLevel = i;
        vkCmdBlitImage(commandBuffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }
    barrier.subresourceRange.baseMipLevel = miplevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    vkutil::endSigleTimeCommands(engine,commandBuffer);
}

bool vkutil::load_image_from_file(VulkanEngine* engine, const char* file, AllocatedImage& outImage) {
    int texWidth, texHeight, texChannels;

    stbi_uc* pixels = stbi_load(file, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels) {
        std::cout << "Failed to load texture file " << file << std::endl;
        return false;
    }
    void* pixel_ptr = pixels;
    VkDeviceSize imageSize = texWidth * texHeight * 4;
   // uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texHeight, texWidth)))) + 1;

    //the format R8G8B8A8 matches exactly with the pixels loaded from stb_image lib
    VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;

    //allocate temporary buffer for holding texture data to upload
    AllocatedBuffer  stagingBuffer = engine->create_buffer(
        true,
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY);
    //copy data to buffer
    void* data;
    vmaMapMemory(engine->_allocator, stagingBuffer._allocation, &data);
    memcpy(data, pixel_ptr, static_cast<size_t>(imageSize));
    vmaUnmapMemory(engine->_allocator, stagingBuffer._allocation);
    //we no longer need the loaded data, so we can free the pixels as they are now in the staging buffer
    stbi_image_free(pixels);

    VkExtent3D imageExtent;
    imageExtent.width = static_cast<uint32_t>(texWidth);
    imageExtent.height = static_cast<uint32_t>(texHeight);
    imageExtent.depth = 1;

    VkImageCreateInfo dimg_info = vkinit::image_create_info(
        imageExtent,
        1,
        image_format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    AllocatedImage newImage;
    VmaAllocationCreateInfo dimg_allocinfo = {};
    dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    //allocate and create the image
    vmaCreateImage(engine->_allocator, &dimg_info, &dimg_allocinfo, &newImage._image, &newImage._allocation, nullptr);
    //add image destroy function to main deletion queue

    engine->_mainDeletionQueue.push_function([=]() {
        vmaDestroyImage(engine->_allocator, newImage._image, newImage._allocation);
        std::cout << "newImage" << std::endl;
        });

    //submit transfer image layout command buffer
    engine->immediate_submit([&](VkCommandBuffer cmd) {
        //adjust image undefined layout to optimal transfer destination layout
        //set pipeline barrier
        vkutil::adjustImageLayout(
            cmd,
            newImage._image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1
        );
        //set copy buffer data to image command buffer
        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = imageExtent;
        //copy the buffer into the image
        vkCmdCopyBufferToImage(
            cmd,
            stagingBuffer._buffer,
            newImage._image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copyRegion);
        //adjust image optimal transfer destination layout to vertex shader read only layout
        vkutil::adjustImageLayout(
            cmd,
            newImage._image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            1
        );
        });
    //NOTE: remember destroy staging buffer
    vmaDestroyBuffer(engine->_allocator, stagingBuffer._buffer, stagingBuffer._allocation);

    std::cout << "Texture loaded successfully " << file << std::endl;

    outImage = newImage;
    return true;

}

bool vkutil::load_image_from_file(VulkanEngine* engine, const char* file, AllocatedImage& outImage, uint32_t& mipLevels) {
    int texWidth, texHeight, texChannels;

    stbi_uc* pixels = stbi_load(file, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels) {
        std::cout << "Failed to load texture file " << file << std::endl;
        return false;
    }
    void* pixel_ptr = pixels;
    VkDeviceSize imageSize = texWidth * texHeight * 4;
     mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texHeight, texWidth)))) + 1;
     //the format R8G8B8A8 matches exactly with the pixels loaded from stb_image lib
     VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;

     //allocate temporary buffer for holding texture data to upload
     AllocatedBuffer  stagingBuffer = engine->create_buffer(
         true,
         imageSize,
         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
         VMA_MEMORY_USAGE_CPU_ONLY);
     //copy data to buffer
     void* data;
     vmaMapMemory(engine->_allocator, stagingBuffer._allocation, &data);
     memcpy(data, pixel_ptr, static_cast<size_t>(imageSize));
     vmaUnmapMemory(engine->_allocator, stagingBuffer._allocation);
     //we no longer need the loaded data, so we can free the pixels as they are now in the staging buffer
     stbi_image_free(pixels);

     VkExtent3D imageExtent;
     imageExtent.width = static_cast<uint32_t>(texWidth);
     imageExtent.height = static_cast<uint32_t>(texHeight);
     imageExtent.depth = 1;

     VkImageCreateInfo dimg_info = vkinit::image_create_info(
         imageExtent,
         mipLevels,
         image_format,
         VK_IMAGE_TILING_OPTIMAL,
         VK_IMAGE_USAGE_TRANSFER_SRC_BIT | 
         VK_IMAGE_USAGE_SAMPLED_BIT | 
         VK_IMAGE_USAGE_TRANSFER_DST_BIT);

     AllocatedImage newImage;
     VmaAllocationCreateInfo dimg_allocinfo = {};
     dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
     //allocate and create the image
     vmaCreateImage(engine->_allocator, &dimg_info, &dimg_allocinfo, &newImage._image, &newImage._allocation, nullptr);
     //add image destroy function to main deletion queue
     engine->_mainDeletionQueue.push_function([=]() {
         vmaDestroyImage(engine->_allocator, newImage._image, newImage._allocation);
         std::cout << "newImage" << std::endl;
         });
     //VkCommandBuffer cmd = engine->_uploadContext._commandBuffer;
     VkCommandBuffer cmd = vkutil::beginSigleTimeCommands(engine);
     //submit transfer image layout command buffer
     //adjust image undefined layout to optimal transfer destination layout
     //set pipeline barrier
     vkutil::adjustImageLayout(
         cmd,
         newImage._image,
         VK_IMAGE_LAYOUT_UNDEFINED,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         mipLevels
     );
     //set copy buffer data to image command buffer
     vkutil::copyBufferToImage(engine, cmd, stagingBuffer._buffer, newImage._image, imageExtent);

     vkutil::generateMipmaps(engine, newImage._image, dimg_info);

     //NOTE: remember destroy staging buffer
     vmaDestroyBuffer(engine->_allocator, stagingBuffer._buffer, stagingBuffer._allocation);

     std::cout << "Texture loaded successfully " << file << std::endl;
     outImage = newImage;
     return true;
}














