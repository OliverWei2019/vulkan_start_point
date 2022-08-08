#pragma once
#ifndef VK_MESH_H
#define VK_MESH_H
#include <vk_types.h>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp> //now needed for the Vertex struct
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

struct MeshPushConstants {
    glm::vec4 data;
    glm::mat4 render_matrix;
};

struct VertexInputDescription {

    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {

    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;
    static VertexInputDescription get_vertex_description();
};

struct Mesh {
    std::vector<Vertex> _vertices;

    AllocatedBuffer _vertexBuffer;
    bool load_from_obj(const char* filename);
};

struct UploadContext {
    VkFence _uploadFence;
    VkCommandPool _commandPool;
    VkCommandBuffer _commandBuffer;
};

#endif // !VK_MESH_H
