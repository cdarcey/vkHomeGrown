# Disclaimer
- Heavily under construction, things will change often and likely break often
- Im very new to Vulkan and am building this project to learn it so most things will likely not be done great for now
- Im not sure how far I will take this at this point in time

# vkHomeGrown

![License](https://img.shields.io/badge/License-MIT-blue.svg)
![Language](https://img.shields.io/badge/Language-C-blue.svg)
![Vulkan](https://img.shields.io/badge/API-Vulkan-orange.svg)
![Status](https://img.shields.io/badge/Status-Active-green.svg)

A lightweight, educational Vulkan rendering framework written in C. Designed for learning graphics programming while providing practical, reusable abstractions over the Vulkan API.

## 📑 Table of Contents
- [Overview](#overview)
- [Features](#features)
- [Getting Started](#getting-started)
- [Project Structure](#project-structure)
- [Learning Resources](#learning-resources)
- [API Reference](#api-reference)
- [Examples](#examples)
- [Limitations](#limitations)
- [Acknowledgments](#acknowledgments)
- [Related Projects](#related-projects)

## 📋 Overview

**vkHomeGrown** is not a full game engine, but a minimalistic rendering layer that helps you understand Vulkan from the ground up. It provides sensible abstractions for common rendering tasks while keeping the Vulkan concepts visible and understandable.

Perfect for:
- Learning Vulkan without drowning in boilerplate
- Building small graphics demos and prototypes
- Understanding modern GPU APIs
- Educational graphics programming projects

## ✨ Features

- **Minimalistic API**     - Just enough abstraction to be useful, not enough to hide Vulkan's concepts
- **Educational Design**   - Clear separation of concerns, well-commented code (at least I tried)
- **Modern C**             - Clean, readable code following consistent patterns
- **GLFW Integration**     - Cross-platform window management
- **Resource Management**  - Vertex buffers, index buffers, textures, pipelines
- **Swapchain Management** - Automatic recreation on window resize
- **Descriptor Support**   - Texture sampling and uniform buffers
- **Build System**         - Simple .bat setup for easy compilation

## 🚀 Getting Started

### Prerequisites
- C compiler (GCC, Clang, or MSVC)
- Vulkan SDK
- GLFW 3.x

### Installation

```bash
# Still working on a more portable solution (I use a custom batch file)
# Clone the repository
git clone https://github.com/username/vkHomeGrown.git
cd vkHomeGrown

```

### Quick Start Example

```c


#include "vkHomeGrown.h"


int main(void) 
{
    // init GLFW
    if (!glfwInit()) 
    {
        printf("Failed to initialize GLFW!\n");
        return -1;
    }

    // tell GLFW not to create OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // create window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan Application", NULL, NULL);
    if (!window) 
    {
        printf("Failed to create GLFW window!\n");
        glfwTerminate();
        return -1;
    }

    // init app state
    hgAppData tState = {0};
    tState.pWindow = window;  // store GLFW window pointer
    tState.width = 800;
    tState.height = 600;

    // get actual window size (framebuffer size for Vulkan)
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    tState.width = fbWidth;
    tState.height = fbHeight;

    // fixed resources
    hg_create_instance(&tState, "test app", VK_MAKE_VERSION(1, 0, 0), true);
    hg_create_surface(&tState);
    hg_pick_physical_device(&tState);
    hg_create_logical_device(&tState);


    // test vertex & index data
    float fTestVertices[] = {
        // x,   y,    r,   g,   b,   a,   u,   v
        -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  // bottom-left  -> red
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f,  // bottom-right -> green
         0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,  // top-right    -> blue
        -0.5f,  0.5f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f   // top-left     -> yellow
    };
    uint16_t uTestIndices[6] = {
        0, 1, 2,  // first triangle  (BL, BR, TR)
        2, 3, 0   // second triangle (TR, TL, BL)
    };


    hgRenderPassConfig tConfig = {
        .tLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .tStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
        .afClearColor = {1.0f, 1.0f, 1.0f, 1.0f}
    };

    hg_create_swapchain(&tState, VK_PRESENT_MODE_FIFO_KHR);
    hg_create_render_pass(&tState, &tConfig);

    hg_create_framebuffers(&tState);
    hg_create_command_pool(&tState);

    hgVertexBuffer tTestVertBuffer = hg_create_vertex_buffer(&tState, fTestVertices, sizeof(fTestVertices), sizeof(float) * 8);
    hgIndexBuffer  tTestIndBuffer  = hg_create_index_buffer(&tState, uTestIndices, 6);


    // descriptors 
    VkDescriptorPool      tDescPool            = VK_NULL_HANDLE;
    VkDescriptorSetLayout tDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet       tDescriptorSet       = VK_NULL_HANDLE;

    // desc pool
    VkDescriptorPoolSize tDescPoolSize[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
    };

    const VkDescriptorPoolCreateInfo tDescPoolCreateInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = 300,
        .poolSizeCount = 1,
        .pPoolSizes    = tDescPoolSize,
    };
    VULKAN_CHECK(vkCreateDescriptorPool(tState.tContextComponents.tDevice, &tDescPoolCreateInfo, NULL, &tDescPool));

    // sets and layouts
    VkDescriptorSetLayoutBinding tTextureAttachmentBinding = {
        .binding         = 0,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT
    };
    VkDescriptorSetLayoutCreateInfo tDescriptorLayoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings    = &tTextureAttachmentBinding
    };
    VULKAN_CHECK(vkCreateDescriptorSetLayout(tState.tContextComponents.tDevice, &tDescriptorLayoutInfo, NULL, &tDescriptorSetLayout));

    // allocate
    const VkDescriptorSetAllocateInfo tDescSetAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = tDescPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &tDescriptorSetLayout
    };
    VULKAN_CHECK(vkAllocateDescriptorSets(tState.tContextComponents.tDevice, &tDescSetAllocInfo, &tDescriptorSet));

    // texture loading and sampler creation
    int iTextureHeight = 0;
    int iTextureWidth  = 0;
    unsigned char* pcTextureData = hg_load_texture_data("../textures/cobble.png", &iTextureWidth, &iTextureHeight);
    hgTexture tTestTexture = hg_create_texture(&tState, pcTextureData, iTextureWidth, iTextureHeight);

    VkSamplerCreateInfo tSamplerInfo = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = VK_FILTER_LINEAR,
        .minFilter               = VK_FILTER_LINEAR,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable        = VK_FALSE,
        .maxAnisotropy           = 1.0f,
        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias              = 0.0f,
        .minLod                  = 0.0f,
        .maxLod                  = 0.0f
    };
    VkSampler tTextureSampler;
    vkCreateSampler(tState.tContextComponents.tDevice, &tSamplerInfo, NULL, &tTextureSampler);

    // update descriptor set with texture
    VkDescriptorImageInfo tImageInfo = {
        .sampler     = tTextureSampler,
        .imageView   = tTestTexture.tImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkWriteDescriptorSet tDescriptorWrite = {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = tDescriptorSet,
        .dstBinding      = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo      = &tImageInfo
    };
    vkUpdateDescriptorSets(tState.tContextComponents.tDevice, 1, &tDescriptorWrite, 0, NULL);


    // tests for new pipeline creation
    VkVertexInputAttributeDescription tTestVertAttribs[3] = {
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,       .offset = 0},                 // pos
        {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = sizeof(float) * 2}, // color
        {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,       .offset = sizeof(float) * 6}  // texcoord
    };

    hgPipelineConfig tTestConfig = {
        .pcVertexShaderPath        = "./shaders/vert.spv",
        .pcFragmentShaderPath      = "./shaders/frag.spv",
        .uVertexStride             = sizeof(hgVertex),
        .ptAttributeDescriptions   = tTestVertAttribs,
        .uAttributeCount           = 3,
        .bBlendEnable              = VK_FALSE,
        .tCullMode                 = VK_CULL_MODE_FRONT_BIT,
        .tFrontFace                = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .tTopology                 = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .ptDescriptorSetLayouts    = &tDescriptorSetLayout,
        .uDescriptorSetLayoutCount = 1,
        .ptPushConstantRanges      = NULL,
        .uPushConstantRangeCount   = 0
    };
    hgPipeline tTestPipeline = hg_create_graphics_pipeline(&tState, &tTestConfig);

    hg_create_sync_objects(&tState);

    // command buffer creation and recording
    hg_allocate_frame_cmd_buffers(&tState);

    // main loop
    while (!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();

        int newWidth, newHeight;
        glfwGetFramebufferSize(window, &newWidth, &newHeight);

        // handle resize or minimization (width and height are 0)
        if (newWidth != tState.width || newHeight != tState.height || newWidth == 0 || newHeight == 0) 
        {
            // wait for device to finish current work
            vkDeviceWaitIdle(tState.tContextComponents.tDevice);

            // recreate swapchain and dependent resources
            hg_recreate_swapchain(&tState);
            continue; // skip this frame
        }

        // begin frame
        uint32_t uImageIndex = hg_begin_frame(&tState);
        hg_begin_render_pass(&tState, uImageIndex);


        // scene/ frame building -> testing stuff in here for now
        vkCmdBindPipeline(tState.tCommandComponents.tCommandBuffers[uImageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, tTestPipeline.tPipeline);
        if(tDescriptorSet != VK_NULL_HANDLE) 
        {
            vkCmdBindDescriptorSets(tState.tCommandComponents.tCommandBuffers[uImageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, 
                tTestPipeline.tPipelineLayout, 0, 1, &tDescriptorSet, 0, NULL);
        }
        // bind vertex buffer
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(tState.tCommandComponents.tCommandBuffers[uImageIndex], 0, 1, &tTestVertBuffer.tBuffer, offsets);
        vkCmdBindIndexBuffer(tState.tCommandComponents.tCommandBuffers[uImageIndex], tTestIndBuffer.tBuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(tState.tCommandComponents.tCommandBuffers[uImageIndex], tTestIndBuffer.uIndexCount, 1, 0, 0, 0);

        // end frame 
        hg_end_render_pass(&tState);
        hg_end_frame(&tState, tState.tCommandComponents.uCurrentImageIndex);

    }


    // cleanup
    vkDeviceWaitIdle(tState.tContextComponents.tDevice);  // wait before cleanup
    hg_cleanup(&tState);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
```

## 📁 Project Structure

```
📁 vkHomeGrown/
├── 📁 src/
│   ├── vkHomeGrown.h    # Main public API header
│   ├── vkHomeGrown.c    # Implementation
│   ├── build.bat        # build script
│   └── main.c           # Example application
├── 📁 shaders/          # SPIR-V shader files
│   ├── vert.spv
│   └── frag.spv
├── 📁 textures/         # Example texture assets
├── 📄 README.md         # This file
└── 📄 LICENSE           # MIT License
```

## 🎓 Learning Resources

This project is designed to be educational. Key concepts covered:

1. **Vulkan Initialization** - Instance, device, queues
2. **Swapchain Management** - Images, image views, presentation
3. **Pipeline Creation** - Shaders, vertex input, rasterization state
4. **Memory Management** - Buffers, images, memory allocation
5. **Command Recording** - Command buffers, render passes, synchronization
6. **Resource Binding** - Descriptors, vertex/index buffers

## 🔧 API Reference

### Core Types
- `hgAppData` - Main application state container
- `hgVertexBuffer`, `hgIndexBuffer` - Geometry buffers
- `hgTexture` - Image texture with Vulkan resources
- `hgPipeline` - Graphics pipeline state

### Initialization
- `hg_create_instance()` - Create Vulkan instance
- `hg_create_surface()` - Create window surface
- `hg_pick_physical_device()` - Select GPU
- `hg_create_logical_device()` - Create logical device

### Resource Creation
- `hg_create_vertex_buffer()` - Upload vertex data to GPU
- `hg_create_index_buffer()` - Upload index data to GPU
- `hg_create_texture()` - Create and upload texture
- `hg_create_graphics_pipeline()` - Create graphics pipeline

### Frame Rendering
- `hg_begin_frame()` - Start frame, acquire swapchain image
- `hg_end_frame()` - Submit commands and present
- `hg_begin_render_pass()` - Start rendering to framebuffer
- `hg_end_render_pass()` - End rendering pass

## 🎯 Example Projects

coming soon...


## ⚠️ Limitations

This is an educational project with some intentional limitations:

- Single queue family (graphics + presentation)
- No depth buffering (coming soon)
- Basic synchronization (single frame in flight)
- Limited error handling for clarity
- No advanced features (compute, ray tracing)


## 🙏 Acknowledgments

- The Vulkan Working Group and Khronos Group
- GLFW developers for excellent windowing library
- All the Vulkan tutorials and documentation that made this possible
- The open source graphics programming community
- Special thanks to [Jonathan Hoffstadt](https://github.com/hoffstadt) 


## 🔗 Related Projects

- [Vulkan Tutorial](https://vulkan-tutorial.com/) - Great learning resource
- [GLFW](https://www.glfw.org/) - Window and input management
- [stb_image](https://github.com/nothings/stb) - Image loading
- [Dear ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI


**vkHomeGrown** - Grow your Vulkan knowledge from the ground up!