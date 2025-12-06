

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

    hgVertex* atVertices = malloc(sizeof(hgVertex) * 16);  // 4 vertices per quad * 4 quads = 16 vertices
    uint16_t* atIndices = malloc(sizeof(uint16_t) * 24);   // 6 indices per quad * 4 quads = 24 indices

    // Quad 0: Bottom-left quadrant
    atVertices[0] = (hgVertex){-1.0f, -1.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 0.0f};  // bottom-left
    atVertices[1] = (hgVertex){ 0.0f, -1.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 0.0f};  // bottom-right
    atVertices[2] = (hgVertex){ 0.0f,  0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f};  // top-right
    atVertices[3] = (hgVertex){-1.0f,  0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f};  // top-left

    // Quad 1: Bottom-right quadrant
    atVertices[4] = (hgVertex){ 0.0f, -1.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 0.0f};  // bottom-left
    atVertices[5] = (hgVertex){ 1.0f, -1.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 0.0f};  // bottom-right
    atVertices[6] = (hgVertex){ 1.0f,  0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f};  // top-right
    atVertices[7] = (hgVertex){ 0.0f,  0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f};  // top-left

    // Quad 2: Top-left quadrant
    atVertices[8]  = (hgVertex){-1.0f,  0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 0.0f};  // bottom-left
    atVertices[9]  = (hgVertex){ 0.0f,  0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 0.0f};  // bottom-right
    atVertices[10] = (hgVertex){ 0.0f,  1.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f};  // top-right
    atVertices[11] = (hgVertex){-1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f};  // top-left

    // Quad 3: Top-right quadrant
    atVertices[12] = (hgVertex){ 0.0f,  0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 0.0f};  // bottom-left
    atVertices[13] = (hgVertex){ 1.0f,  0.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 0.0f};  // bottom-right
    atVertices[14] = (hgVertex){ 1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f};  // top-right
    atVertices[15] = (hgVertex){ 0.0f,  1.0f,  1.0f, 1.0f, 1.0f, 1.0f,  0.0f, 1.0f};  // top-left

    // Indices for all 4 quads (each quad uses 6 indices)
    // Quad 0 indices
    atIndices[0] = 0; atIndices[1] = 1; atIndices[2] = 2;
    atIndices[3] = 2; atIndices[4] = 3; atIndices[5] = 0;

    // Quad 1 indices
    atIndices[6] = 4; atIndices[7] = 5; atIndices[8] = 6;
    atIndices[9] = 6; atIndices[10] = 7; atIndices[11] = 4;

    // Quad 2 indices
    atIndices[12] = 8; atIndices[13] = 9; atIndices[14] = 10;
    atIndices[15] = 10; atIndices[16] = 11; atIndices[17] = 8;

    // Quad 3 indices
    atIndices[18] = 12; atIndices[19] = 13; atIndices[20] = 14;
    atIndices[21] = 14; atIndices[22] = 15; atIndices[23] = 12;

    // vkHomeGrown api init funcs
    hg_create_instance(&tState);
    hg_create_surface(&tState);
    hg_pick_physical_device(&tState);
    hg_create_logical_device(&tState);
    hg_create_swapchain(&tState);
    hg_create_render_pass(&tState);
    hg_create_graphics_pipeline(&tState);
    hg_create_framebuffers(&tState);
    hg_create_command_pool(&tState);
    hg_create_vertex_buffer(&tState, atVertices, atIndices, 16, 24);

    *tState.tResources.tDescriptorSets = malloc(sizeof(VkDescriptorSet));
    tState.tResources.tDescriptorSets[0] = VK_NULL_HANDLE;


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
    VULKAN_CHECK(vkCreateDescriptorPool(tState.tContextComponents.tDevice, &tDescPoolCreateInfo, NULL, &tState.tResources.tDescriptorPool));

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
    VULKAN_CHECK(vkCreateDescriptorSetLayout(tState.tContextComponents.tDevice, &tDescriptorLayoutInfo, NULL, &tState.tResources.tDescriptorSetLayout));

    // allocate
    const VkDescriptorSetAllocateInfo tDescSetAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = tState.tResources.tDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &tState.tResources.tDescriptorSetLayout
    };
    VULKAN_CHECK(vkAllocateDescriptorSets(tState.tContextComponents.tDevice, &tDescSetAllocInfo, tState.tResources.tDescriptorSets));

    int iTextureHeight = 0;
    int iTextureWidth  = 0;
    unsigned char* pcTextureData = hg_load_texture_data("../textures/cobble.png", &iTextureWidth, &iTextureHeight);

    hgTexture tTestTexture = hg_create_texture(&tState, pcTextureData, iTextureWidth, iTextureHeight);

    VkSamplerCreateInfo tSamplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 0.0f
    };
    VkSampler tTextureSampler;
    vkCreateSampler(tState.tContextComponents.tDevice, &tSamplerInfo, NULL, &tTextureSampler);

    // update descriptor set with texture
    VkDescriptorImageInfo tImageInfo = {
        .sampler = tTextureSampler,
        .imageView = tTestTexture.tImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkWriteDescriptorSet tDescriptorWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = *tState.tResources.tDescriptorSets,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &tImageInfo
    };

    vkUpdateDescriptorSets(tState.tContextComponents.tDevice, 1, &tDescriptorWrite, 0, NULL);
    
    

    hg_create_command_buffers(&tState);
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

    free(atVertices);
    free(atIndices);

    // cleanup
    vkDeviceWaitIdle(tState.tContextComponents.tDevice);  // wait before cleanup
    hg_cleanup(&tState);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}