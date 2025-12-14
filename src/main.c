

#define _USE_MATH_DEFINES

#include "vkHomeGrown.h"
#include <math.h>
#include <string.h>

void processInput(GLFWwindow *window, float* fAngularVelocity, float* fAngVelConst);

int main(void) 
{
    // ============================================================================
    // INITIALIZATION
    // ============================================================================

    // render settings
    bool bTextured = true;

    // initialize glfw
    if (!glfwInit()) 
    {
        printf("Failed to initialize GLFW!\n");
        return -1;
    }

    // configure glfw for vulkan (no opengl context)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // create window
    GLFWwindow* window = glfwCreateWindow(1200, 800, "Vulkan Application", NULL, NULL);
    if(!window) 
    {
        printf("Failed to create GLFW window!\n");
        glfwTerminate();
        return -1;
    }

    // initialize application state
    hgAppData tState = {0};
    tState.pWindow       = window;
    tState.width         = 800;
    tState.height        = 600;
    tState.bDepthEnabled = false;

    // get actual framebuffer size
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    tState.width  = fbWidth;
    tState.height = fbHeight;

    // create vulkan core components
    hg_create_instance(&tState, "test app", VK_MAKE_VERSION(1, 0, 0), true);
    hg_create_surface(&tState);
    hg_pick_physical_device(&tState);
    hg_create_logical_device(&tState);

    // ============================================================================
    // GEOMETRY DATA
    // ============================================================================

    // quad vertices: position (x,y), color (rgba), texcoord (uv)
    float fTestVerticesQuad[] = {
        // x, y,        r, g, b, a,             u, v
        -0.25f, -0.25f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // top left
        -0.25f,  0.25f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, // bottom left
         0.25f,  0.25f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, // bottom right
         0.25f, -0.25f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f  // top right
    };

    // quad indices for two triangles
    uint16_t uTestIndices[6] = {
        0, 1, 2,  // first triangle
        2, 3, 0   // second triangle
    };

    // ============================================================================
    // ANIMATION VARIABLES
    // ============================================================================

    // angular velocities for orbit and spin
    float fOrbitAngularVelocity = (2 * M_PI) / 5;  // complete orbit in 5 seconds
    float fSpinAngularVelocity  = (2 * M_PI) / 3;   // complete spin in 3 seconds
    float fAngVelConst          = fOrbitAngularVelocity;    // for resetting to original speed

    // current angles
    float fOrbitAngle = 0.0f;
    float fSpinAngle  = 0.0f;

    // orbit parameters
    float fRadius = 0.5f;  // orbit radius

    // for tracking time
    float fLastFrameTime = 0.0f;

    // temporary buffer for transformed vertex data
    float fVertexDataCopy[sizeof(fTestVerticesQuad) / sizeof(float)] = {0};

    // ============================================================================
    // VULKAN RESOURCES
    // ============================================================================

    // render pass configuration
    hgRenderPassConfig tConfig = {
        .tLoadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .tStoreOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .afClearColor = {1.0f, 1.0f, 1.0f, 1.0f}
    };

    // create rendering pipeline components
    hg_create_swapchain(&tState, VK_PRESENT_MODE_MAILBOX_KHR);
    hg_create_render_pass(&tState, &tConfig);
    hg_create_framebuffers(&tState);
    hg_create_command_pool(&tState);

    // create vertex and index buffers (dynamic buffer for cpu updates)
    hgVertexBuffer fTestVertBufferQuad = hg_create_dynamic_vertex_buffer(&tState, fTestVerticesQuad, sizeof(fTestVerticesQuad), sizeof(float) * 8);
    hgIndexBuffer  tTestIndBuffer      = hg_create_index_buffer(&tState, uTestIndices, 6);

    // ============================================================================
    // TEXTURE SETUP
    // ============================================================================

    // descriptor pool for texture samplers
    VkDescriptorPoolSize tPoolSize = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 50};
    VkDescriptorPool tDescPool     = hg_create_descriptor_pool(&tState, 100, &tPoolSize, 1);

    VkDescriptorSetLayout tDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet       tDescriptorSet       = VK_NULL_HANDLE;

    // descriptor set layout for texture binding
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

    // allocate descriptor set
    const VkDescriptorSetAllocateInfo tDescSetAllocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = tDescPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &tDescriptorSetLayout
    };
    VULKAN_CHECK(vkAllocateDescriptorSets(tState.tContextComponents.tDevice, &tDescSetAllocInfo, &tDescriptorSet));

    // load texture from file
    int iTextureHeight = 0;
    int iTextureWidth  = 0;
    unsigned char* pcTextureData = hg_load_texture_data("../textures/cobble.png", &iTextureWidth, &iTextureHeight);
    hgTexture      tTestTexture  = hg_create_texture(&tState, pcTextureData, iTextureWidth, iTextureHeight);

    // create texture sampler
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

    // bind texture to descriptor set
    hg_update_texture_descriptor(&tState, tDescriptorSet, 0, &tTestTexture, tTextureSampler);

    // ============================================================================
    // GRAPHICS PIPELINE
    // ============================================================================

    // vertex attribute descriptions
    VkVertexInputAttributeDescription tTestVertAttribs[3] = {
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,       .offset = 0},                 // position
        {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = sizeof(float) * 2}, // color
        {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT,       .offset = sizeof(float) * 6}  // texcoord
    };

    // pipeline configuration
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
        .tPipelineBindPoint        = VK_PIPELINE_BIND_POINT_GRAPHICS
    };
    hgPipeline tTestPipeline = hg_create_graphics_pipeline(&tState, &tTestConfig);

    // create synchronization objects
    hg_create_sync_objects(&tState);

    // allocate command buffers (frame specific)
    hg_allocate_frame_cmd_buffers(&tState);

    // ============================================================================
    // PRE-RENDER SETUP
    // ============================================================================

    // set vertex colors to white so texture displays true to source
    if(bTextured)
    {
        for(uint32_t i = 0; i < fTestVertBufferQuad.uVertexCount * 8; i += 8) 
        {
            fTestVerticesQuad[i + 2] = 1.0f; // r
            fTestVerticesQuad[i + 3] = 1.0f; // g
            fTestVerticesQuad[i + 4] = 1.0f; // b
        }
        memcpy(fTestVertBufferQuad.pDataMapped, fTestVerticesQuad, sizeof(fVertexDataCopy));
    }

    // ============================================================================
    // MAIN RENDER LOOP
    // ============================================================================

    while(!glfwWindowShouldClose(window)) 
    {
        // process window events
        glfwPollEvents();
        processInput(window, &fOrbitAngularVelocity, &fAngVelConst);

        // check for window resize
        int newWidth, newHeight;
        glfwGetFramebufferSize(window, &newWidth, &newHeight);

        if (newWidth != tState.width || newHeight != tState.height || newWidth == 0 || newHeight == 0) 
        {
            vkDeviceWaitIdle(tState.tContextComponents.tDevice);
            hg_recreate_swapchain(&tState);
            continue;
        }

        // ------------------------------------------------------------------------
        // animation update
        // ------------------------------------------------------------------------

        // calculate delta time
        float fCurrentTime = glfwGetTime();
        float fDeltaTime   = fCurrentTime - fLastFrameTime; 
        fLastFrameTime     = fCurrentTime;

        // update rotation angles
        fOrbitAngle -= fOrbitAngularVelocity * fDeltaTime;
        fSpinAngle  += fSpinAngularVelocity * fDeltaTime;

        // copy original vertex data
        memcpy(fVertexDataCopy, fTestVerticesQuad, sizeof(fTestVerticesQuad));

        // apply transformations to each vertex
        for(uint32_t i = 0; i < fTestVertBufferQuad.uVertexCount * 8; i += 8) 
        {
            // extract original vertex position
            float fOrigX = fTestVerticesQuad[i];
            float fOrigY = fTestVerticesQuad[i + 1];

            // apply spin rotation around origin
            float fRotatedX = fOrigX * cosf(fSpinAngle) - fOrigY * sinf(fSpinAngle);
            float fRotatedY = fOrigX * sinf(fSpinAngle) + fOrigY * cosf(fSpinAngle);

            // calculate orbit offset
            float fOrbitOffsetX = fRadius * cosf(fOrbitAngle);
            float fOrbitOffsetY = fRadius * sinf(fOrbitAngle);

            // combine spin and orbit transformations
            fVertexDataCopy[i]     = fRotatedX + fOrbitOffsetX;
            fVertexDataCopy[i + 1] = fRotatedY + fOrbitOffsetY;
        }
        // update gpu buffer with transformed vertices
        memcpy(fTestVertBufferQuad.pDataMapped, fVertexDataCopy, sizeof(fVertexDataCopy));

        // ------------------------------------------------------------------------
        // rendering
        // ------------------------------------------------------------------------

        // begin frame and render pass
        uint32_t uImageIndex = hg_begin_frame(&tState);
        hg_begin_render_pass(&tState, uImageIndex);

        // bind graphics pipeline
        vkCmdBindPipeline(tState.tCommandComponents.tCommandBuffers[uImageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, tTestPipeline.tPipeline);

        // bind texture descriptor set if using textures
        if(bTextured) 
        {
            vkCmdBindDescriptorSets(tState.tCommandComponents.tCommandBuffers[uImageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, tTestPipeline.tPipelineLayout, 0, 1, &tDescriptorSet, 0, NULL);
        }

        // bind vertex and index buffers
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(tState.tCommandComponents.tCommandBuffers[uImageIndex], 0, 1, &fTestVertBufferQuad.tBuffer, offsets);
        vkCmdBindIndexBuffer(tState.tCommandComponents.tCommandBuffers[uImageIndex], tTestIndBuffer.tBuffer, 0, VK_INDEX_TYPE_UINT16);

        // draw quad
        vkCmdDrawIndexed(tState.tCommandComponents.tCommandBuffers[uImageIndex], tTestIndBuffer.uIndexCount, 1, 0, 0, 0);

        // end render pass and frame
        hg_end_render_pass(&tState);
        hg_end_frame(&tState, tState.tCommandComponents.uCurrentImageIndex);
    }

    // ============================================================================
    // CLEANUP
    // ============================================================================
    
    // wait for all operations to complete
    vkDeviceWaitIdle(tState.tContextComponents.tDevice);

    // destroy resources in reverse order of creation
    hg_destroy_texture(&tState, &tTestTexture);
    hg_destroy_vertex_buffer(&tState, &fTestVertBufferQuad);
    hg_destroy_index_buffer(&tState, &tTestIndBuffer);
    hg_destroy_pipeline(&tState, &tTestPipeline);

    // destroy descriptor resources
    vkDestroyDescriptorPool(tState.tContextComponents.tDevice, tDescPool, NULL);
    vkDestroyDescriptorSetLayout(tState.tContextComponents.tDevice, tDescriptorSetLayout, NULL);
    vkDestroySampler(tState.tContextComponents.tDevice, tTextureSampler, NULL);

    // cleanup core components
    hg_core_cleanup(&tState);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

void 
processInput(GLFWwindow *window, float* fAngularVelocity, float* fAngVelConst)
{
    // decrease orbit speed (not frame-independent, happens every frame)
    if(glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
    {
        if(*fAngularVelocity >= 0.00001f)
            *fAngularVelocity -= 0.05f;
    }

    // increase orbit speed
    if(glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
    {
        *fAngularVelocity += 0.05f;
    }
    // reset to original speed
    if(glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        *fAngularVelocity = *fAngVelConst;
    }
}