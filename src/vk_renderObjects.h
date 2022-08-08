#pragma once
#ifndef VK_RENDER_OBJECTS_H
#define VK_RENDER_OBJECTS_H
//add unordered_map to the headers on top
#include <vk_types.h>
#include <vk_mesh.h>
#include <unordered_map>
//note that we store the VkPipeline and layout by value, not pointer.
//They are 64 bit handles to internal driver structures anyway so storing pointers to them isn't very useful

struct Material {
	VkDescriptorSet textureSet{ VK_NULL_HANDLE }; //texture defaulted to null
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
};

struct RenderObject {
	Mesh* mesh;

	Material* material;

	glm::mat4 transformMatrix;
};
struct RenderObjectsSets {
	//default array of renderable objects
	std::vector<RenderObject> _renderables;

	std::unordered_map<std::string, Material> _materials;
	std::unordered_map<std::string, Mesh> _meshes;
	//functions

	//create material and add it to the map
	Material* create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);

	//returns nullptr if it can't be found
	Material* get_material(const std::string& name);

	//returns nullptr if it can't be found
	Mesh* get_mesh(const std::string& name);
};
#endif // ! VK_RENDER_OBJECTS_H
