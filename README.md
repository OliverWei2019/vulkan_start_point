# vulkan_start_point
这是一个学习vulkan的初级项目，这是旧的文件，我创建这个库用来保留之前的学习项目，清缓存，本地的文件不留了。
主体框架来自于[vkguide](https://vkguide.dev)。  

![维京人的小屋](Vikings_Room.PNG)

## 
<details><summary>实施理念和想法</summary>
<p>
	
[vkguide](https://vkguide.dev)中没有MSAA和mipmap，我增加了MSAA和生成mipmap函数，另外使用我之前写的一些封装函数[VK_API_ENCA](https://github.com/OliverWei2019/Vulkan_Api_Enca)。  

</p>

</details>

## 
<details><summary>RUN</summary>
<p>

### Cmake+Win10+VS2019
	
使用CmakeGUI编译，新建一个build文件夹，编译后的文件解决方案文件将存放在build文件夹下.  
查看运行效果可以直接点击```bin/Debug/```目录下的Vulkan_guide.exe文件

</p>
</details>


## 
<details><summary>文件说明</summary>
<p>

 ##
<details><summary>assets</summary>
<p>

## assets
这里放了一些我自己测试使用的资产文件，这些文件来自于[vkguide](https://vkguide.dev), 不包括README展示的维京人的小屋模型，需要手动导入。
 </p>
 </details>
 
  ##
<details><summary>bin/Debug</summary>
<p>

## bin/Debug
这是本机编译出来的文件，vulkan_guide.exe可以查看效果
 </p>
 </details>
 
 
 ##
 <details><summary>src</summary>
 <p>
 
## src
 
 主要的实现文件都在这里，包括几个大方面
 ### engine
 vk_engine.cpp包含了一些vulkan的初始化，imgui的创建，资产加载，场景初始化，以及渲染过程.   
 vk_engine_scenerender.cpp是具体的渲染过程函数这部分需要和vk_scene.cpp结合。  
 ### scene
 vk_scene.cpp是一个重要的预处理文件，在实现简介绘制和bindless之前，需要提前将所有的资产和处理流程打包，这是用scene部分不负责完成的，将所有的资产和流程打包封装好后，可以交给vk_engine_scenerender.cpp处理。
 ### textture,material,mesh
 vk_texture.cpp负责加载texture,优先从assets_export加载AsserFile文件.  
 vk_mesh.cpp负责加载mesh,以及一些简单预处理函数，例如计算包围盒，法线封装(八面体)。  
 material_system.cpp,这个文件很重要与vk_scene.cpp和vk_shader.cpp关系密切，负责一些简单渲染管线的生成和提前构建效果模板。  
 ### initialization,descriptor,pushbuffer,play_camera
 vk_initializers.cpp vulkan有太太太多的对象结构需要填充信息，独立出一个initializer负责初始化，这是很常见的步骤。  
 vk_descriptor.cpp 将描述符相关的对象进行封装，以便快速的构建描述符集和布局。  
 vk_pushbuffer.cpp 对于需要单独推送一些buffer给GPU,使用这个文件中的函数，特别是imgui需要展示的信息，例外这里面有意一个数据对齐函数很重要，来自[SaschaWillems](https://github.com/SaschaWillems/Vulkan/tree/master/examples/dynamicuniformbuffer)。  
 play_camera.cpp 就是运动相机功能，与SDL库结合处理相机运动
 ### cvars和profiler
 cvars.cpp 是一个全局的控制变量系统，相关知识可以从这里：[CVARS](https://vkguide.dev/docs/extra-chapter/cvar_system/)获取。  
 vk_profiler.cpp 是debug所需要的一部分函数,需要一个库[Tracy Profiler](https://github.com/wolfpld/tracy)，负责探查不同处理流程的耗时，类似于监控帧率。  
 ### shader
 vk_shader.cpp 负责加载shader模型以及反射spv文件，需要一个库[SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect)。
 spv反射能解决很大部分手里劳动:grin:   

</p>
</details>

##
<details><summary>shaders</summary>
<p>

## shaders

放了glsl格式shader文件，.spv文件是通过CmakeList命令编译出来的，把shader文件后缀写成.vert,.frag或者.comp即可。  


</p>
</details>

</p>
</details>

##
## 感谢开源库:grin:  
 
GUI调试： [Imgui](https://github.com/ocornut/imgui)。   。  
着色器库： [glm](https://github.com/g-truc/glm)  。   。   
图像加载： [stb_image](https://github.com/nothings/stb)  。  
OBJ文件加载： [tinyObjLoader](https://github.com/tinyobjloader/tinyobjloader)  。    
AMD的Vulkan内存管理： [VMA](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)  。  

