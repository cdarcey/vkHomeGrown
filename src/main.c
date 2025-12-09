

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
        .tCullMode                 = VK_CULL_MODE_NONE,
        .tFrontFace                = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .tTopology                 = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .ptDescriptorSetLayouts    = &tDescriptorSetLayout,
        .uDescriptorSetLayoutCount = 1,
        .ptPushConstantRanges      = NULL,
        .uPushConstantRangeCount   = 0
    };
    hgPipeline tTestPipeline = hg_create_graphics_pipeline(&tState, &tTestConfig);

    // command buffer creation and recording
    tState.tCommandComponents.tCommandBuffers = malloc(tState.tSwapchainComponents.uSwapchainImageCount * sizeof(VkCommandBuffer));

    VkCommandBufferAllocateInfo tAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = tState.tCommandComponents.tCommandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = tState.tSwapchainComponents.uSwapchainImageCount
    };
    VULKAN_CHECK(vkAllocateCommandBuffers(tState.tContextComponents.tDevice, &tAllocInfo, tState.tCommandComponents.tCommandBuffers));

    // record command buffers
    for(uint32_t i = 0; i < tState.tSwapchainComponents.uSwapchainImageCount; i++) 
    {
        VkCommandBufferBeginInfo tBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };
        VULKAN_CHECK(vkBeginCommandBuffer(tState.tCommandComponents.tCommandBuffers[i], &tBeginInfo));

        VkClearValue tClearColor = {
            .color = {{1.0f, 1.0f, 1.0f, 1.0f}}  // RGBA format
        };

        VkRenderPassBeginInfo tRenderPassInfo = {
            .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass  = tState.tPipelineComponents.tRenderPass,
            .framebuffer = tState.tPipelineComponents.tFramebuffers[i],
            .renderArea  = {
                .offset  = {0, 0},
                .extent  = tState.tSwapchainComponents.tExtent
            },
            .clearValueCount = 1,
            .pClearValues    = &tClearColor
        };
        vkCmdBeginRenderPass(tState.tCommandComponents.tCommandBuffers[i], &tRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(tState.tCommandComponents.tCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, tTestPipeline.tPipeline);

        if(tDescriptorSet != NULL || tDescriptorSet != VK_NULL_HANDLE) 
        {
            vkCmdBindDescriptorSets(tState.tCommandComponents.tCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, 
                tTestPipeline.tPipelineLayout, 0, 1, &tDescriptorSet, 0, NULL);
        }

        // bind vertex buffer
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(tState.tCommandComponents.tCommandBuffers[i], 0, 1, &tTestVertBuffer.tBuffer, offsets);
        vkCmdBindIndexBuffer(tState.tCommandComponents.tCommandBuffers[i], tTestIndBuffer.tBuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(tState.tCommandComponents.tCommandBuffers[i], tTestIndBuffer.uIndexCount, 1, 0, 0, 0);

        vkCmdEndRenderPass(tState.tCommandComponents.tCommandBuffers[i]);
        VULKAN_CHECK(vkEndCommandBuffer(tState.tCommandComponents.tCommandBuffers[i]));
    }
    hg_create_sync_objects(&tState);


    // main loop
    while (!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();  // handle window events

        // check for window resize
        int newWidth, newHeight;
        glfwGetFramebufferSize(window, &newWidth, &newHeight);
        if (newWidth != tState.width || newHeight != tState.height) 
        {
            tState.width = newWidth;
            tState.height = newHeight;
            // TODO: Handle swapchain recreation for resize
        }

        // render frame
        hg_draw_frame(&tState);
    }


    // cleanup
    vkDeviceWaitIdle(tState.tContextComponents.tDevice);  // wait before cleanup
    hg_cleanup(&tState);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}