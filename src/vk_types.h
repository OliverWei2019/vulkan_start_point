// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

//use vma allocate buffer include VkBuffer and VmaAllocation
struct AllocatedBuffer {
    VkBuffer _buffer;
    VmaAllocation _allocation;
};
// use vma allocate image include VkImage and VmaAllcation
struct AllocatedImage {
    VkImage _image;
    VmaAllocation _allocation;
};
struct Texture {
    AllocatedImage image;
    VkImageView imageView;
};
//we will add our main reusable types here