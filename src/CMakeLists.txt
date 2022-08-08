find_package(lz4 CONFIG REQUIRED)
# Add source to this project's executable.
add_executable(vulkan_guide
    main.cpp
    vk_engine.cpp
    vk_engine.h
    vk_types.h
    vk_initializers.cpp
    vk_initializers.h
    vk_pipelines.cpp
    vk_pipelines.h
    vk_renderObjects.cpp
    vk_renderObjects.h
    vk_mesh.cpp
    vk_mesh.h
    vk_frameData.cpp
    vk_frameData.h
    vk_texture.cpp
    vk_texture.h
    cvar_system.cpp
    cvar_system.h
    string_utils.h
    vk_descriptor.cpp
    vk_descriptor.h
)

set_property(TARGET vulkan_guide PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_guide>")

target_include_directories(vulkan_guide PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(vulkan_guide vkbootstrap vma glm tinyobjloader imgui stb_image)

target_link_libraries(vulkan_guide Vulkan::Vulkan sdl2)
target_link_libraries(vulkan_guide lz4::lz4)
add_dependencies(vulkan_guide Shaders)
