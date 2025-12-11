

#include "vkHomeGrown.h"


int main(void) 
{

    // example settings
    bool bQuad     = true;  // false renders a triangle
    bool bTextured = false; // false renders colors in vert data 

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


    // test vertex & index data for quad
    float fTestVerticesQuad[] = {
        // x, y,      r, g, b, a,             u, v
        -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,   // top left     -> red
        -0.5f,  0.5f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,   // bottom left  -> yellow
         0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,   // bottom right -> blue
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f    // top right    -> green
    };
    uint16_t uTestIndices[6] = {
        0, 1, 2,  // first triangle  (TL, BL, BR)
        2, 3, 0   // second triangle (BR, TR, TL)
    };

    // test vertex data for triangle 
    float fTestVerticesTriangle[] = {
        // x, y,      r, g, b, a,              u, v
         0.0f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.0f,   // top          -> red
        -0.5f,  0.5f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,   // bottom left  -> green
         0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,   // bottom right -> blue
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

    hgVertexBuffer fTestVertBufferQuad     = hg_create_vertex_buffer(&tState, fTestVerticesQuad, sizeof(fTestVerticesQuad), sizeof(float) * 8);
    hgVertexBuffer fTestVertBufferTriangle = hg_create_vertex_buffer(&tState, fTestVerticesTriangle, sizeof(fTestVerticesTriangle), sizeof(float) * 8);
    hgIndexBuffer  tTestIndBuffer        = hg_create_index_buffer(&tState, uTestIndices, 6);


    // descriptors 
    VkDescriptorPoolSize tPoolSize = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 50};
    VkDescriptorPool     tDescPool = hg_create_descriptor_pool(&tState, 100, &tPoolSize, 1);


    VkDescriptorSetLayout tDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet       tDescriptorSet       = VK_NULL_HANDLE;

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

    // texture loading 
    int iTextureHeight = 0;
    int iTextureWidth  = 0;
    unsigned char* pcTextureData = hg_load_texture_data("../textures/cobble.png", &iTextureWidth, &iTextureHeight);
    hgTexture tTestTexture = hg_create_texture(&tState, pcTextureData, iTextureWidth, iTextureHeight);

    // sampler
    VkSampler tTextureSampler;
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
    vkCreateSampler(tState.tContextComponents.tDevice, &tSamplerInfo, NULL, &tTextureSampler);

    hg_update_texture_descriptor(&tState, tDescriptorSet, 0, &tTestTexture, tTextureSampler);

    // tests for new pipeline creation
    VkVertexInputAttributeDescription tTestVertAttribs[3] = {
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,       .offset = 0},                 // pos
        {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = sizeof(float) * 2}, // color
        {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,       .offset = sizeof(float) * 6}  // texcoord
    };

    hgPipelineConfig tTestConfig = {
        .pcVertexShaderPath        = bTextured ? "../out/shaders/textured_vert.spv" : "../out/shaders/not_textured_vert.spv",
        .pcFragmentShaderPath      = bTextured ? "../out/shaders/textured_frag.spv" : "../out/shaders/not_textured_frag.spv",
        .uVertexStride             = sizeof(float) * 8,
        .ptAttributeDescriptions   = tTestVertAttribs,
        .uAttributeCount           = 3,
        .bBlendEnable              = VK_FALSE,
        .tCullMode                 = VK_CULL_MODE_BACK_BIT,
        .tFrontFace                = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .ptDescriptorSetLayouts    = bTextured ? &tDescriptorSetLayout : NULL,
        .uDescriptorSetLayoutCount = bTextured ? 1 : 0,
        .ptPushConstantRanges      = NULL,
        .uPushConstantRangeCount   = 0,
        .tPipelineBindPoint        = VK_PIPELINE_BIND_POINT_GRAPHICS // for pipeline binding
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

    
        if(bTextured) // apply texture 
        {
            vkCmdBindDescriptorSets(tState.tCommandComponents.tCommandBuffers[uImageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, tTestPipeline.tPipelineLayout, 0, 1, &tDescriptorSet, 0, NULL);
        }

        if(bQuad) // render quad
        {
            // bind vertex buffer
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(tState.tCommandComponents.tCommandBuffers[uImageIndex], 0, 1, &fTestVertBufferQuad.tBuffer, offsets);
            vkCmdBindIndexBuffer(tState.tCommandComponents.tCommandBuffers[uImageIndex], tTestIndBuffer.tBuffer, 0, VK_INDEX_TYPE_UINT16);
            vkCmdDrawIndexed(tState.tCommandComponents.tCommandBuffers[uImageIndex], tTestIndBuffer.uIndexCount, 1, 0, 0, 0);
        }
        else // render triangle
        {
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(tState.tCommandComponents.tCommandBuffers[uImageIndex], 0, 1, &fTestVertBufferTriangle.tBuffer, offsets);
            vkCmdDraw(tState.tCommandComponents.tCommandBuffers[uImageIndex], 3, 1, 0, 0);
        }
        // end frame 
        hg_end_render_pass(&tState);
        hg_end_frame(&tState, tState.tCommandComponents.uCurrentImageIndex);

    }

    // cleanup
    vkDeviceWaitIdle(tState.tContextComponents.tDevice);  // wait before cleanup

    // destroy low level resources first 
    hg_destroy_texture(&tState, &tTestTexture); // destroys image, image view, memory
    hg_destroy_vertex_buffer(&tState, &fTestVertBufferQuad);
    hg_destroy_vertex_buffer(&tState, &fTestVertBufferTriangle);
    hg_destroy_index_buffer(&tState, &tTestIndBuffer);

    // destroy pipeline 
    hg_destroy_pipeline(&tState, &tTestPipeline); // destroys pipeline + pipeline layout

    // vulkan clean up that have no helpers (the resources will be managed by the api user)
    vkDestroyDescriptorPool(tState.tContextComponents.tDevice, tDescPool, NULL);
    vkDestroyDescriptorSetLayout(tState.tContextComponents.tDevice, tDescriptorSetLayout, NULL);
    vkDestroySampler(tState.tContextComponents.tDevice, tTextureSampler, NULL);

    // should be called after all other cleanup
    hg_core_cleanup(&tState);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}